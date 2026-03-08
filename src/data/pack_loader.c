#include "data/pack_loader.h"

#include <stdio.h>
#include <string.h>

#include "core_pack.h"

typedef struct Vf2dHeaderCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float cell_size;
    uint32_t obstacle_mask_crc32;
} Vf2dHeaderCanonical;

typedef struct DawHeaderCanonical {
    uint32_t version;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t samples_per_pixel;
    uint64_t point_count;
    uint64_t start_frame;
    uint64_t end_frame;
    uint64_t project_duration_frames;
} DawHeaderCanonical;

typedef struct TraceHeaderCanonical {
    uint32_t trace_profile_version;
    uint32_t reserved;
    uint64_t sample_count;
    uint64_t marker_count;
} TraceHeaderCanonical;

void datalab_frame_init(DatalabFrame *frame) {
    if (!frame) return;
    memset(frame, 0, sizeof(*frame));
    frame->profile = DATALAB_PROFILE_UNKNOWN;
}

void datalab_frame_free(DatalabFrame *frame) {
    if (!frame) return;
    core_free(frame->density);
    core_free(frame->velx);
    core_free(frame->vely);
    core_free(frame->wave_min);
    core_free(frame->wave_max);
    core_free(frame->markers);
    core_free(frame->trace_samples);
    core_free(frame->trace_markers);
    core_free(frame->manifest_json);
    memset(frame, 0, sizeof(*frame));
}

static CoreResult read_float_chunk(CorePackReader *reader,
                                   const CorePackChunkInfo *chunk,
                                   float **out_values,
                                   size_t expected_count) {
    if (!reader || !chunk || !out_values) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    const size_t expected_bytes = expected_count * sizeof(float);
    if (chunk->size != (uint64_t)expected_bytes) {
        CoreResult r = { CORE_ERR_FORMAT, "unexpected field chunk size" };
        return r;
    }

    float *values = (float *)core_alloc(expected_bytes);
    if (!values) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    CoreResult rr = core_pack_reader_read_chunk_data(reader, chunk, values, chunk->size);
    if (rr.code != CORE_OK) {
        core_free(values);
        return rr;
    }

    *out_values = values;
    return core_result_ok();
}

static CoreResult read_marker_chunk(CorePackReader *reader,
                                    const CorePackChunkInfo *chunk,
                                    DatalabDawMarker **out_markers,
                                    size_t *out_count) {
    if (!reader || !chunk || !out_markers || !out_count) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (chunk->size % sizeof(DatalabDawMarker) != 0) {
        CoreResult r = { CORE_ERR_FORMAT, "invalid marker chunk size" };
        return r;
    }

    size_t count = (size_t)(chunk->size / sizeof(DatalabDawMarker));
    if (count == 0) {
        *out_markers = NULL;
        *out_count = 0;
        return core_result_ok();
    }

    DatalabDawMarker *markers = (DatalabDawMarker *)core_alloc((size_t)chunk->size);
    if (!markers) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    CoreResult rr = core_pack_reader_read_chunk_data(reader, chunk, markers, chunk->size);
    if (rr.code != CORE_OK) {
        core_free(markers);
        return rr;
    }

    *out_markers = markers;
    *out_count = count;
    return core_result_ok();
}

static CoreResult read_optional_manifest(CorePackReader *reader, DatalabFrame *out_frame) {
    CorePackChunkInfo json;
    CoreResult fj = core_pack_reader_find_chunk(reader, "JSON", 0, &json);
    if (fj.code != CORE_OK || json.size == 0) {
        return core_result_ok();
    }

    char *manifest = (char *)core_alloc((size_t)json.size + 1u);
    if (!manifest) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    CoreResult rj = core_pack_reader_read_chunk_data(reader, &json, manifest, json.size);
    if (rj.code != CORE_OK) {
        core_free(manifest);
        return rj;
    }

    manifest[json.size] = '\0';
    out_frame->manifest_json = manifest;
    out_frame->manifest_size = (size_t)json.size;
    return core_result_ok();
}

static CoreResult load_physics_profile(CorePackReader *reader, const CorePackChunkInfo *vfhd, DatalabFrame *out_frame) {
    if (!reader || !vfhd || !out_frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (vfhd->size < sizeof(Vf2dHeaderCanonical)) {
        CoreResult r = { CORE_ERR_FORMAT, "vfhd chunk too small" };
        return r;
    }

    Vf2dHeaderCanonical hdr;
    memset(&hdr, 0, sizeof(hdr));
    CoreResult read_hdr = core_pack_reader_read_chunk_data(reader, vfhd, &hdr, sizeof(hdr));
    if (read_hdr.code != CORE_OK) {
        return read_hdr;
    }

    out_frame->profile = DATALAB_PROFILE_PHYSICS;
    out_frame->vf_version = hdr.version;
    out_frame->width = hdr.grid_w;
    out_frame->height = hdr.grid_h;
    out_frame->time_seconds = hdr.time_seconds;
    out_frame->frame_index = hdr.frame_index;
    out_frame->dt_seconds = hdr.dt_seconds;
    out_frame->origin_x = hdr.origin_x;
    out_frame->origin_y = hdr.origin_y;
    out_frame->cell_size = hdr.cell_size;
    out_frame->obstacle_mask_crc32 = hdr.obstacle_mask_crc32;

    const size_t count = (size_t)out_frame->width * (size_t)out_frame->height;

    CorePackChunkInfo dens;
    CorePackChunkInfo velx;
    CorePackChunkInfo vely;

    CoreResult fd = core_pack_reader_find_chunk(reader, "DENS", 0, &dens);
    CoreResult fx = core_pack_reader_find_chunk(reader, "VELX", 0, &velx);
    CoreResult fy = core_pack_reader_find_chunk(reader, "VELY", 0, &vely);
    if (fd.code != CORE_OK || fx.code != CORE_OK || fy.code != CORE_OK) {
        CoreResult r = { CORE_ERR_NOT_FOUND, "required field chunks missing" };
        return r;
    }

    CoreResult rd = read_float_chunk(reader, &dens, &out_frame->density, count);
    if (rd.code != CORE_OK) {
        return rd;
    }

    CoreResult rx = read_float_chunk(reader, &velx, &out_frame->velx, count);
    if (rx.code != CORE_OK) {
        return rx;
    }

    CoreResult ry = read_float_chunk(reader, &vely, &out_frame->vely, count);
    if (ry.code != CORE_OK) {
        return ry;
    }

    return read_optional_manifest(reader, out_frame);
}

static CoreResult load_daw_profile(CorePackReader *reader, const CorePackChunkInfo *dawh, DatalabFrame *out_frame) {
    if (!reader || !dawh || !out_frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (dawh->size < sizeof(DawHeaderCanonical)) {
        CoreResult r = { CORE_ERR_FORMAT, "dawh chunk too small" };
        return r;
    }

    DawHeaderCanonical hdr;
    memset(&hdr, 0, sizeof(hdr));
    CoreResult read_hdr = core_pack_reader_read_chunk_data(reader, dawh, &hdr, sizeof(hdr));
    if (read_hdr.code != CORE_OK) {
        return read_hdr;
    }

    out_frame->profile = DATALAB_PROFILE_DAW;
    out_frame->daw_version = hdr.version;
    out_frame->sample_rate = hdr.sample_rate;
    out_frame->channels = hdr.channels;
    out_frame->samples_per_pixel = hdr.samples_per_pixel;
    out_frame->point_count = hdr.point_count;
    out_frame->start_frame = hdr.start_frame;
    out_frame->end_frame = hdr.end_frame;
    out_frame->project_duration_frames = hdr.project_duration_frames;

    if (hdr.point_count == 0) {
        CoreResult r = { CORE_ERR_FORMAT, "invalid daw point count" };
        return r;
    }

    CorePackChunkInfo wmin;
    CorePackChunkInfo wmax;
    CorePackChunkInfo mrks;

    CoreResult fmin = core_pack_reader_find_chunk(reader, "WMIN", 0, &wmin);
    CoreResult fmax = core_pack_reader_find_chunk(reader, "WMAX", 0, &wmax);
    CoreResult fmrk = core_pack_reader_find_chunk(reader, "MRKS", 0, &mrks);
    if (fmin.code != CORE_OK || fmax.code != CORE_OK || fmrk.code != CORE_OK) {
        CoreResult r = { CORE_ERR_NOT_FOUND, "required daw chunks missing" };
        return r;
    }

    CoreResult rmin = read_float_chunk(reader, &wmin, &out_frame->wave_min, (size_t)hdr.point_count);
    if (rmin.code != CORE_OK) {
        return rmin;
    }

    CoreResult rmax = read_float_chunk(reader, &wmax, &out_frame->wave_max, (size_t)hdr.point_count);
    if (rmax.code != CORE_OK) {
        return rmax;
    }

    CoreResult rmk = read_marker_chunk(reader, &mrks, &out_frame->markers, &out_frame->marker_count);
    if (rmk.code != CORE_OK) {
        return rmk;
    }

    return read_optional_manifest(reader, out_frame);
}

static CoreResult load_trace_profile(CorePackReader *reader, const CorePackChunkInfo *trhd, DatalabFrame *out_frame) {
    TraceHeaderCanonical hdr;
    CorePackChunkInfo trsm;
    CorePackChunkInfo trev;
    CoreResult rr;

    if (!reader || !trhd || !out_frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (trhd->size < sizeof(TraceHeaderCanonical)) {
        CoreResult r = { CORE_ERR_FORMAT, "trhd chunk too small" };
        return r;
    }

    memset(&hdr, 0, sizeof(hdr));
    rr = core_pack_reader_read_chunk_data(reader, trhd, &hdr, sizeof(hdr));
    if (rr.code != CORE_OK) {
        return rr;
    }

    out_frame->profile = DATALAB_PROFILE_TRACE;
    out_frame->trace_version = hdr.trace_profile_version;
    out_frame->trace_sample_count = (size_t)hdr.sample_count;
    out_frame->trace_marker_count = (size_t)hdr.marker_count;

    if (hdr.sample_count > 0u) {
        rr = core_pack_reader_find_chunk(reader, "TRSM", 0, &trsm);
        if (rr.code != CORE_OK) {
            CoreResult r = { CORE_ERR_NOT_FOUND, "required trace samples chunk missing" };
            return r;
        }
        if (trsm.size != hdr.sample_count * sizeof(DatalabTraceSample)) {
            CoreResult r = { CORE_ERR_FORMAT, "invalid trsm chunk size" };
            return r;
        }
        out_frame->trace_samples = (DatalabTraceSample *)core_alloc((size_t)trsm.size);
        if (!out_frame->trace_samples) {
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }
        rr = core_pack_reader_read_chunk_data(reader, &trsm, out_frame->trace_samples, trsm.size);
        if (rr.code != CORE_OK) {
            return rr;
        }
    }

    if (hdr.marker_count > 0u) {
        rr = core_pack_reader_find_chunk(reader, "TREV", 0, &trev);
        if (rr.code != CORE_OK) {
            CoreResult r = { CORE_ERR_NOT_FOUND, "required trace marker chunk missing" };
            return r;
        }
        if (trev.size != hdr.marker_count * sizeof(DatalabTraceMarker)) {
            CoreResult r = { CORE_ERR_FORMAT, "invalid trev chunk size" };
            return r;
        }
        out_frame->trace_markers = (DatalabTraceMarker *)core_alloc((size_t)trev.size);
        if (!out_frame->trace_markers) {
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }
        rr = core_pack_reader_read_chunk_data(reader, &trev, out_frame->trace_markers, trev.size);
        if (rr.code != CORE_OK) {
            return rr;
        }
    }

    return read_optional_manifest(reader, out_frame);
}

CoreResult datalab_load_pack(const char *pack_path, DatalabFrame *out_frame) {
    if (!pack_path || !out_frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    datalab_frame_init(out_frame);

    CorePackReader reader;
    CoreResult open_r = core_pack_reader_open(pack_path, &reader);
    if (open_r.code != CORE_OK) {
        return open_r;
    }

    out_frame->chunk_count = core_pack_reader_chunk_count(&reader);

    CorePackChunkInfo vfhd;
    CorePackChunkInfo dawh;
    CorePackChunkInfo trhd;
    CoreResult find_vfhd = core_pack_reader_find_chunk(&reader, "VFHD", 0, &vfhd);
    CoreResult find_dawh = core_pack_reader_find_chunk(&reader, "DAWH", 0, &dawh);
    CoreResult find_trhd = core_pack_reader_find_chunk(&reader, "TRHD", 0, &trhd);

    CoreResult load_r;
    if (find_trhd.code == CORE_OK) {
        load_r = load_trace_profile(&reader, &trhd, out_frame);
    } else if (find_vfhd.code == CORE_OK) {
        load_r = load_physics_profile(&reader, &vfhd, out_frame);
    } else if (find_dawh.code == CORE_OK) {
        load_r = load_daw_profile(&reader, &dawh, out_frame);
    } else {
        load_r.code = CORE_ERR_NOT_FOUND;
        load_r.message = "unsupported pack profile (missing TRHD/VFHD/DAWH)";
    }

    if (load_r.code != CORE_OK) {
        datalab_frame_free(out_frame);
        core_pack_reader_close(&reader);
        return load_r;
    }

    core_pack_reader_close(&reader);
    return core_result_ok();
}

void datalab_print_frame_summary(const char *pack_path, const DatalabFrame *frame) {
    if (!frame) {
        return;
    }

    printf("pack=%s\n", pack_path ? pack_path : "<unknown>");
    printf("  profile=%s chunks=%zu\n",
           frame->profile == DATALAB_PROFILE_PHYSICS ? "physics" :
           frame->profile == DATALAB_PROFILE_DAW ? "daw" :
           frame->profile == DATALAB_PROFILE_TRACE ? "trace" : "unknown",
           frame->chunk_count);

    if (frame->profile == DATALAB_PROFILE_PHYSICS) {
        printf("  grid=%ux%u frame=%llu time=%.6f dt=%.6f\n",
               frame->width,
               frame->height,
               (unsigned long long)frame->frame_index,
               frame->time_seconds,
               frame->dt_seconds);
        printf("  origin=(%.3f,%.3f) cell=%.3f crc=%u\n",
               frame->origin_x,
               frame->origin_y,
               frame->cell_size,
               frame->obstacle_mask_crc32);
    } else if (frame->profile == DATALAB_PROFILE_DAW) {
        printf("  daw_version=%u sample_rate=%u channels=%u samples_per_pixel=%u\n",
               frame->daw_version,
               frame->sample_rate,
               frame->channels,
               frame->samples_per_pixel);
        printf("  points=%llu markers=%zu start=%llu end=%llu project_end=%llu\n",
               (unsigned long long)frame->point_count,
               frame->marker_count,
               (unsigned long long)frame->start_frame,
               (unsigned long long)frame->end_frame,
               (unsigned long long)frame->project_duration_frames);
    } else if (frame->profile == DATALAB_PROFILE_TRACE) {
        printf("  trace_version=%u samples=%zu markers=%zu\n",
               frame->trace_version,
               frame->trace_sample_count,
               frame->trace_marker_count);
    }

    if (frame->manifest_json) {
        printf("  manifest_bytes=%zu\n", frame->manifest_size);
    }
}

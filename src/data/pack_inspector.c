#include "data/pack_inspector.h"

#include <stdio.h>
#include <string.h>

#include "core_pack.h"

static int datalab_pack_inspection_has_type(const DatalabPackInspection *inspection, const char *type) {
    size_t i = 0u;
    if (!inspection || !type) {
        return 0;
    }
    for (i = 0u; i < inspection->listed_chunk_count; ++i) {
        if (strcmp(inspection->chunks[i].type, type) == 0) {
            return 1;
        }
    }
    return 0;
}

static void datalab_pack_inspection_classify(DatalabPackInspection *inspection) {
    if (!inspection) {
        return;
    }
    if (datalab_pack_inspection_has_type(inspection, "VF3H")) {
        snprintf(inspection->family, sizeof(inspection->family), "PhysicsSim VF3D");
    } else if (datalab_pack_inspection_has_type(inspection, "GFHD")) {
        snprintf(inspection->family, sizeof(inspection->family), "GrowthSim field frame");
    } else if (datalab_pack_inspection_has_type(inspection, "VFHD")) {
        snprintf(inspection->family, sizeof(inspection->family), "Physics field 2D");
    } else if (datalab_pack_inspection_has_type(inspection, "LDHD")) {
        snprintf(inspection->family, sizeof(inspection->family), "LineDrawing diagnostics");
    } else if (datalab_pack_inspection_has_type(inspection, "DAWH")) {
        snprintf(inspection->family, sizeof(inspection->family), "DAW waveform");
    } else if (datalab_pack_inspection_has_type(inspection, "TRHD")) {
        snprintf(inspection->family, sizeof(inspection->family), "Trace series");
    } else {
        snprintf(inspection->family, sizeof(inspection->family), "Generic core pack");
    }
}

CoreResult datalab_inspect_pack(const char *path, DatalabPackInspection *out_inspection) {
    CorePackReader reader = {0};
    CoreResult result;
    size_t i = 0u;
    if (!path || !path[0] || !out_inspection) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid pack inspection request" };
    }
    memset(out_inspection, 0, sizeof(*out_inspection));
    result = core_pack_reader_open(path, &reader);
    if (result.code != CORE_OK) {
        return result;
    }
    out_inspection->version_major = reader.version_major;
    out_inspection->version_minor = reader.version_minor;
    out_inspection->chunk_count = core_pack_reader_chunk_count(&reader);
    out_inspection->listed_chunk_count = out_inspection->chunk_count;
    if (out_inspection->listed_chunk_count > DATALAB_PACK_INSPECTION_MAX_CHUNKS) {
        out_inspection->listed_chunk_count = DATALAB_PACK_INSPECTION_MAX_CHUNKS;
    }
    for (i = 0u; i < out_inspection->listed_chunk_count; ++i) {
        CorePackChunkInfo chunk = {{0}, 0u, 0u};
        result = core_pack_reader_get_chunk(&reader, i, &chunk);
        if (result.code != CORE_OK) {
            (void)core_pack_reader_close(&reader);
            memset(out_inspection, 0, sizeof(*out_inspection));
            return result;
        }
        snprintf(out_inspection->chunks[i].type, sizeof(out_inspection->chunks[i].type), "%s", chunk.type);
        out_inspection->chunks[i].size = chunk.size;
    }
    datalab_pack_inspection_classify(out_inspection);
    return core_pack_reader_close(&reader);
}

void datalab_pack_inspection_format_summary(const DatalabPackInspection *inspection,
                                            char *out_text,
                                            size_t out_text_cap) {
    if (!out_text || out_text_cap == 0u) {
        return;
    }
    if (!inspection) {
        snprintf(out_text, out_text_cap, "PACK INSPECTION UNAVAILABLE");
        return;
    }
    snprintf(out_text, out_text_cap, "%s | v%u.%u | %zu CHUNKS",
             inspection->family, inspection->version_major, inspection->version_minor,
             inspection->chunk_count);
}

void datalab_pack_inspection_format_chunk(const DatalabPackInspection *inspection,
                                          size_t chunk_index,
                                          char *out_text,
                                          size_t out_text_cap) {
    if (!out_text || out_text_cap == 0u) {
        return;
    }
    if (!inspection || chunk_index >= inspection->listed_chunk_count) {
        out_text[0] = '\0';
        return;
    }
    snprintf(out_text, out_text_cap, "%s  %llu BYTES", inspection->chunks[chunk_index].type,
             (unsigned long long)inspection->chunks[chunk_index].size);
}

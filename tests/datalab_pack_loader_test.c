#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core_pack.h"
#include "data/pack_loader.h"

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

typedef struct DatalabDawMarkerDisk {
    uint64_t frame;
    double beat;
    uint32_t kind;
    uint32_t reserved;
    double value_a;
    double value_b;
} DatalabDawMarkerDisk;

typedef struct TraceHeaderCanonical {
    uint32_t trace_profile_version;
    uint32_t reserved;
    uint64_t sample_count;
    uint64_t marker_count;
} TraceHeaderCanonical;

typedef struct TraceSampleDisk {
    double time_seconds;
    float value;
    char lane[32];
} TraceSampleDisk;

typedef struct TraceMarkerDisk {
    double time_seconds;
    char lane[32];
    char label[64];
} TraceMarkerDisk;

typedef struct DrawingLayerDisk {
    uint32_t layer_id;
    char name[32];
    uint8_t visible;
    uint8_t locked;
} DrawingLayerDisk;

typedef struct DrawingDocumentDisk {
    uint32_t schema_version;
    uint32_t logical_width;
    uint32_t logical_height;
    uint32_t sample_density;
    uint32_t layer_count;
    uint32_t next_layer_id;
    uint32_t raster_width;
    uint32_t raster_height;
    uint32_t raster_sample_count;
    DrawingLayerDisk layers[16];
    uint8_t raster_samples[1048576];
} DrawingDocumentDisk;

typedef struct DrawingSnapshotPrefixDisk {
    uint32_t version;
    uint32_t reserved0;
    uint32_t node_count;
    uint32_t binding_count;
    uint32_t history_count;
    uint32_t history_cursor;
    DrawingDocumentDisk document;
} DrawingSnapshotPrefixDisk;

typedef struct DrawingSnapshotShellHeaderDisk {
    uint32_t version;
    uint32_t reserved0;
    uint32_t node_count;
    uint32_t binding_count;
    uint32_t history_count;
    uint32_t history_cursor;
    uint32_t schema_version;
    uint32_t logical_width;
    uint32_t logical_height;
    uint32_t sample_density;
    uint32_t layer_count;
    uint32_t next_layer_id;
    uint32_t raster_width;
    uint32_t raster_height;
    uint32_t raster_sample_count;
} DrawingSnapshotShellHeaderDisk;

typedef struct DrawingSnapshotShellPrefixDisk {
    DrawingSnapshotShellHeaderDisk header;
    DrawingLayerDisk layers[16];
} DrawingSnapshotShellPrefixDisk;

typedef struct DrawingLayerRasterChunkHeaderDisk {
    uint32_t version;
    uint32_t raster_width;
    uint32_t raster_height;
    uint32_t sample_count;
    uint32_t layer_count;
} DrawingLayerRasterChunkHeaderDisk;

typedef struct DrawingPathPointDisk {
    int32_t x;
    int32_t y;
    int32_t handle_in_dx;
    int32_t handle_in_dy;
    int32_t handle_out_dx;
    int32_t handle_out_dy;
    uint8_t bezier_enabled;
    uint8_t handle_linked;
    uint8_t reserved0;
    uint8_t reserved1;
} DrawingPathPointDisk;

typedef struct DrawingObjectChunkHeaderDisk {
    uint32_t version;
    uint32_t object_count;
    uint32_t next_object_id;
    uint32_t reserved0;
} DrawingObjectChunkHeaderDisk;

typedef struct DrawingObjectChunkEntryV3Disk {
    uint32_t object_id;
    uint32_t layer_id;
    uint8_t type;
    uint8_t visible;
    uint8_t locked;
    uint8_t stroke_color_index;
    uint8_t fill_color_index;
    uint8_t stroke_width;
    uint8_t style_mode;
    uint8_t path_closed;
    uint16_t path_point_count;
    int32_t origin_x;
    int32_t origin_y;
    uint32_t width;
    uint32_t height;
    char name[32];
    DrawingPathPointDisk path_points[128];
} DrawingObjectChunkEntryV3Disk;

typedef struct DrawingObjectChunkEntryV4Disk {
    uint32_t object_id;
    uint32_t layer_id;
    uint8_t type;
    uint8_t visible;
    uint8_t locked;
    uint8_t stroke_width;
    uint8_t style_mode;
    uint8_t path_closed;
    uint16_t path_point_count;
    uint16_t reserved0;
    uint32_t stroke_color_value;
    uint32_t fill_color_value;
    int32_t origin_x;
    int32_t origin_y;
    uint32_t width;
    uint32_t height;
    char name[32];
    DrawingPathPointDisk path_points[128];
} DrawingObjectChunkEntryV4Disk;

static void make_test_temp_dir(char *out, size_t out_cap) {
    int n = snprintf(out, out_cap, "/tmp/dl_pack_loader_XXXXXX");
    assert(n > 0);
    assert((size_t)n < out_cap);
    assert(mkdtemp(out) != NULL);
}

static void make_test_fixture_path(char *out, size_t out_cap, const char *dir, const char *name) {
    int n = snprintf(out, out_cap, "%s/%s", dir, name);
    assert(n > 0);
    assert((size_t)n < out_cap);
}

static void cleanup_test_fixture(const char *path) {
    if (path != NULL && path[0] != '\0') {
        (void)remove(path);
    }
}

static uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r << 16u) |
           ((uint32_t)g << 8u) |
           (uint32_t)b |
           ((uint32_t)a << 24u);
}

static void write_physics_pack_with_unknown_chunk(const char *path) {
    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    assert(r.code == CORE_OK);

    Vf2dHeaderCanonical hdr = {0};
    hdr.version = 2;
    hdr.grid_w = 2;
    hdr.grid_h = 2;
    hdr.time_seconds = 1.25;
    hdr.frame_index = 9;
    hdr.dt_seconds = 0.02;
    hdr.cell_size = 1.0f;

    float dens[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float velx[4] = {0.0f, 1.0f, 0.0f, -1.0f};
    float vely[4] = {1.0f, 0.0f, -1.0f, 0.0f};
    const unsigned char unknown_blob[7] = {1, 2, 3, 4, 5, 6, 7};

    assert(core_pack_writer_add_chunk(&w, "VFHD", &hdr, sizeof(hdr)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "DENS", dens, sizeof(dens)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "VELX", velx, sizeof(velx)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "VELY", vely, sizeof(vely)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "XTRA", unknown_blob, sizeof(unknown_blob)).code == CORE_OK);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_daw_pack_with_unknown_chunk(const char *path) {
    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    assert(r.code == CORE_OK);

    DawHeaderCanonical hdr = {0};
    hdr.version = 1;
    hdr.sample_rate = 48000;
    hdr.channels = 2;
    hdr.samples_per_pixel = 256;
    hdr.point_count = 4;
    hdr.start_frame = 0;
    hdr.end_frame = 1023;
    hdr.project_duration_frames = 1024;

    float wmin[4] = {-0.4f, -0.2f, -0.3f, -0.1f};
    float wmax[4] = {0.4f, 0.2f, 0.3f, 0.1f};
    DatalabDawMarkerDisk markers[2] = {
        { .frame = 0, .beat = 0.0, .kind = 1, .reserved = 0, .value_a = 120.0, .value_b = 0.0 },
        { .frame = 512, .beat = 2.0, .kind = 5, .reserved = 0, .value_a = 0.0, .value_b = 0.0 },
    };
    const char json[] = "{\"profile\":\"daw\",\"version\":1}";
    const uint32_t unknown_value = 0xdecafbadU;

    assert(core_pack_writer_add_chunk(&w, "DAWH", &hdr, sizeof(hdr)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "WMIN", wmin, sizeof(wmin)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "WMAX", wmax, sizeof(wmax)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "MRKS", markers, sizeof(markers)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "JSON", json, (uint64_t)(sizeof(json) - 1u)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "ZUNK", &unknown_value, sizeof(unknown_value)).code == CORE_OK);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_trace_pack_with_unknown_chunk(const char *path) {
    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    assert(r.code == CORE_OK);

    TraceHeaderCanonical hdr = {0};
    hdr.trace_profile_version = 1;
    hdr.sample_count = 3;
    hdr.marker_count = 1;

    TraceSampleDisk samples[3];
    memset(samples, 0, sizeof(samples));
    samples[0].time_seconds = 0.1;
    samples[0].value = 0.02f;
    snprintf(samples[0].lane, sizeof(samples[0].lane), "frame_dt");
    samples[1].time_seconds = 0.1;
    samples[1].value = 20.0f;
    snprintf(samples[1].lane, sizeof(samples[1].lane), "solver_iterations");
    samples[2].time_seconds = 0.1;
    samples[2].value = 0.5f;
    snprintf(samples[2].lane, sizeof(samples[2].lane), "density_avg");

    TraceMarkerDisk markers[1];
    memset(markers, 0, sizeof(markers));
    markers[0].time_seconds = 0.1;
    snprintf(markers[0].lane, sizeof(markers[0].lane), "events");
    snprintf(markers[0].label, sizeof(markers[0].label), "trace_start");

    const uint16_t unknown = 0x1234u;

    assert(core_pack_writer_add_chunk(&w, "TRHD", &hdr, sizeof(hdr)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "TRSM", samples, sizeof(samples)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "TREV", markers, sizeof(markers)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "UNKN", &unknown, sizeof(unknown)).code == CORE_OK);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_physics_pack_with_huge_grid(const char *path) {
    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    Vf2dHeaderCanonical hdr = {0};
    assert(r.code == CORE_OK);
    hdr.version = 2;
    hdr.grid_w = UINT32_MAX;
    hdr.grid_h = UINT32_MAX;
    hdr.cell_size = 1.0f;
    assert(core_pack_writer_add_chunk(&w, "VFHD", &hdr, sizeof(hdr)).code == CORE_OK);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_daw_pack_with_huge_point_count(const char *path) {
    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    DawHeaderCanonical hdr = {0};
    assert(r.code == CORE_OK);
    hdr.version = 1;
    hdr.sample_rate = 48000u;
    hdr.channels = 2u;
    hdr.samples_per_pixel = 256u;
    hdr.point_count = UINT64_MAX;
    assert(core_pack_writer_add_chunk(&w, "DAWH", &hdr, sizeof(hdr)).code == CORE_OK);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_trace_pack_with_huge_counts(const char *path) {
    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    TraceHeaderCanonical hdr = {0};
    assert(r.code == CORE_OK);
    hdr.trace_profile_version = 1u;
    hdr.sample_count = UINT64_MAX;
    hdr.marker_count = UINT64_MAX;
    assert(core_pack_writer_add_chunk(&w, "TRHD", &hdr, sizeof(hdr)).code == CORE_OK);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_daw_pack_with_huge_manifest(const char *path) {
    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    DawHeaderCanonical hdr = {0};
    float wmin[1] = { -0.1f };
    float wmax[1] = { 0.1f };
    char *manifest = NULL;
    size_t manifest_size = 9u * 1024u * 1024u;
    assert(r.code == CORE_OK);
    hdr.version = 1;
    hdr.sample_rate = 48000u;
    hdr.channels = 2u;
    hdr.samples_per_pixel = 256u;
    hdr.point_count = 1u;
    manifest = (char *)malloc(manifest_size);
    assert(manifest != NULL);
    memset(manifest, 'x', manifest_size);
    assert(core_pack_writer_add_chunk(&w, "DAWH", &hdr, sizeof(hdr)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "WMIN", wmin, sizeof(wmin)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "WMAX", wmax, sizeof(wmax)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "MRKS", NULL, 0u).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "JSON", manifest, (uint64_t)manifest_size).code == CORE_OK);
    free(manifest);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_sketch_pack(const char *path) {
    CorePackWriter w = {0};
    DrawingSnapshotPrefixDisk *prefix = NULL;
    DrawingLayerRasterChunkHeaderDisk layer_hdr = {0};
    DrawingObjectChunkHeaderDisk object_hdr = {0};
    DrawingObjectChunkEntryV3Disk objects[2];
    uint8_t *object_chunk = NULL;
    uint8_t layer_chunk[20u + (2u * (8u + 256u))];
    uint8_t *cursor = layer_chunk;
    uint8_t eraser = 244u;
    uint8_t layer_samples[2][256];
    CoreResult r = core_pack_writer_open(path, &w);
    assert(r.code == CORE_OK);

    prefix = (DrawingSnapshotPrefixDisk *)calloc(1u, sizeof(*prefix));
    assert(prefix != NULL);
    prefix->version = 1u;
    prefix->document.schema_version = 3u;
    prefix->document.logical_width = 16u;
    prefix->document.logical_height = 16u;
    prefix->document.sample_density = 1u;
    prefix->document.layer_count = 2u;
    prefix->document.next_layer_id = 3u;
    prefix->document.raster_width = 16u;
    prefix->document.raster_height = 16u;
    prefix->document.raster_sample_count = 256u;
    prefix->document.layers[0].layer_id = 1u;
    snprintf(prefix->document.layers[0].name, sizeof(prefix->document.layers[0].name), "Base Layer");
    prefix->document.layers[0].visible = 1u;
    prefix->document.layers[1].layer_id = 2u;
    snprintf(prefix->document.layers[1].name, sizeof(prefix->document.layers[1].name), "Layer 2");
    prefix->document.layers[1].visible = 1u;
    memset(prefix->document.raster_samples, (int)eraser, sizeof(prefix->document.raster_samples));

    memset(layer_samples, (int)eraser, sizeof(layer_samples));
    layer_hdr.version = 1u;
    layer_hdr.raster_width = 16u;
    layer_hdr.raster_height = 16u;
    layer_hdr.sample_count = 256u;
    layer_hdr.layer_count = 2u;
    memcpy(cursor, &layer_hdr, sizeof(layer_hdr));
    cursor += sizeof(layer_hdr);
    memcpy(cursor, &(uint32_t){ 1u }, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    memcpy(cursor, &(uint32_t){ 256u }, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    memcpy(cursor, layer_samples[0], sizeof(layer_samples[0]));
    cursor += sizeof(layer_samples[0]);
    memcpy(cursor, &(uint32_t){ 2u }, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    memcpy(cursor, &(uint32_t){ 256u }, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    memcpy(cursor, layer_samples[1], sizeof(layer_samples[1]));

    memset(objects, 0, sizeof(objects));
    object_hdr.version = 3u;
    object_hdr.object_count = 2u;
    object_hdr.next_object_id = 3u;
    objects[0].object_id = 1u;
    objects[0].layer_id = 2u;
    objects[0].type = 1u;
    objects[0].visible = 1u;
    objects[0].fill_color_index = 7u;
    objects[0].style_mode = 1u;
    objects[0].origin_x = 2;
    objects[0].origin_y = 3;
    objects[0].width = 5u;
    objects[0].height = 4u;
    objects[1].object_id = 2u;
    objects[1].layer_id = 2u;
    objects[1].type = 1u;
    objects[1].visible = 1u;
    objects[1].fill_color_index = 0u;
    objects[1].style_mode = 1u;
    objects[1].origin_x = 9;
    objects[1].origin_y = 8;
    objects[1].width = 3u;
    objects[1].height = 5u;
    object_chunk = (uint8_t *)malloc(sizeof(object_hdr) + sizeof(objects));
    assert(object_chunk != NULL);
    memcpy(object_chunk, &object_hdr, sizeof(object_hdr));
    memcpy(object_chunk + sizeof(object_hdr), objects, sizeof(objects));

    assert(core_pack_writer_add_chunk(&w, "DPS2", prefix, sizeof(*prefix)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "DPLR", layer_chunk, sizeof(layer_chunk)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "DPOB", object_chunk, sizeof(object_hdr) + sizeof(objects)).code == CORE_OK);
    free(object_chunk);
    object_chunk = NULL;
    free(prefix);
    prefix = NULL;
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_sketch_true_color_pack(const char *path) {
    CorePackWriter w = {0};
    DrawingSnapshotPrefixDisk *legacy = NULL;
    DrawingSnapshotShellPrefixDisk shell;
    DrawingLayerRasterChunkHeaderDisk layer_hdr = {0};
    DrawingObjectChunkHeaderDisk object_hdr = {0};
    DrawingObjectChunkEntryV4Disk object_entry;
    uint32_t *base_samples = NULL;
    uint32_t overlay_samples[16];
    uint8_t *layer_chunk = NULL;
    uint8_t *object_chunk = NULL;
    uint8_t *cursor = NULL;
    uint64_t layer_chunk_size = 0u;
    uint64_t object_chunk_size = 0u;
    CoreResult r = core_pack_writer_open(path, &w);
    assert(r.code == CORE_OK);

    legacy = (DrawingSnapshotPrefixDisk *)calloc(1u, sizeof(*legacy));
    assert(legacy != NULL);
    legacy->version = 1u;
    legacy->document.schema_version = 4u;
    legacy->document.logical_width = 4u;
    legacy->document.logical_height = 4u;
    legacy->document.sample_density = 1u;
    legacy->document.layer_count = 2u;
    legacy->document.next_layer_id = 3u;
    legacy->document.raster_width = 4u;
    legacy->document.raster_height = 4u;
    legacy->document.raster_sample_count = 16u;
    legacy->document.layers[0].layer_id = 1u;
    snprintf(legacy->document.layers[0].name, sizeof(legacy->document.layers[0].name), "Base Layer");
    legacy->document.layers[0].visible = 1u;
    legacy->document.layers[1].layer_id = 2u;
    snprintf(legacy->document.layers[1].name, sizeof(legacy->document.layers[1].name), "Overlay");
    legacy->document.layers[1].visible = 1u;

    memset(&shell, 0, sizeof(shell));
    shell.header.version = 2u;
    shell.header.schema_version = 4u;
    shell.header.logical_width = 4u;
    shell.header.logical_height = 4u;
    shell.header.sample_density = 1u;
    shell.header.layer_count = 2u;
    shell.header.next_layer_id = 3u;
    shell.header.raster_width = 4u;
    shell.header.raster_height = 4u;
    shell.header.raster_sample_count = 16u;
    shell.layers[0] = legacy->document.layers[0];
    shell.layers[1] = legacy->document.layers[1];

    base_samples = (uint32_t *)&legacy->document.raster_samples[0];
    for (size_t i = 0; i < 16u; ++i) {
        base_samples[i] = 0u;
        overlay_samples[i] = 0u;
    }
    base_samples[5] = pack_rgba(10u, 20u, 30u, 255u);
    base_samples[10] = pack_rgba(40u, 50u, 60u, 255u);

    layer_hdr.version = 2u;
    layer_hdr.raster_width = 4u;
    layer_hdr.raster_height = 4u;
    layer_hdr.sample_count = 16u;
    layer_hdr.layer_count = 2u;
    layer_chunk_size = 20u + 2u * (8u + 16u * sizeof(uint32_t));
    layer_chunk = (uint8_t *)calloc(1u, (size_t)layer_chunk_size);
    assert(layer_chunk != NULL);
    cursor = layer_chunk;
    memcpy(cursor, &layer_hdr, sizeof(layer_hdr));
    cursor += sizeof(layer_hdr);
    memcpy(cursor, &(uint32_t){ 1u }, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    memcpy(cursor, &(uint32_t){ 16u }, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    memcpy(cursor, base_samples, 16u * sizeof(uint32_t));
    cursor += 16u * sizeof(uint32_t);
    memcpy(cursor, &(uint32_t){ 2u }, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    memcpy(cursor, &(uint32_t){ 16u }, sizeof(uint32_t));
    cursor += sizeof(uint32_t);
    overlay_samples[0] = pack_rgba(220u, 100u, 10u, 255u);
    memcpy(cursor, overlay_samples, 16u * sizeof(uint32_t));

    memset(&object_hdr, 0, sizeof(object_hdr));
    object_hdr.version = 4u;
    object_hdr.object_count = 1u;
    object_hdr.next_object_id = 2u;
    memset(&object_entry, 0, sizeof(object_entry));
    object_entry.object_id = 1u;
    object_entry.layer_id = 2u;
    object_entry.type = 1u;
    object_entry.visible = 1u;
    object_entry.style_mode = 1u;
    object_entry.fill_color_value = pack_rgba(0u, 200u, 255u, 255u);
    object_entry.origin_x = 2;
    object_entry.origin_y = 2;
    object_entry.width = 2u;
    object_entry.height = 2u;
    object_chunk_size = sizeof(object_hdr) + sizeof(object_entry);
    object_chunk = (uint8_t *)calloc(1u, (size_t)object_chunk_size);
    assert(object_chunk != NULL);
    memcpy(object_chunk, &object_hdr, sizeof(object_hdr));
    memcpy(object_chunk + sizeof(object_hdr), &object_entry, sizeof(object_entry));

    assert(core_pack_writer_add_chunk(&w, "DPS3", &shell, sizeof(shell)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "DPS2", legacy, sizeof(*legacy)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "DPLR", layer_chunk, layer_chunk_size).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "DPOB", object_chunk, object_chunk_size).code == CORE_OK);
    free(object_chunk);
    object_chunk = NULL;
    free(layer_chunk);
    layer_chunk = NULL;
    free(legacy);
    legacy = NULL;
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_sketch_pack_with_huge_raster_bounds(const char *path) {
    CorePackWriter w = {0};
    DrawingSnapshotShellPrefixDisk shell;
    CoreResult r = core_pack_writer_open(path, &w);
    assert(r.code == CORE_OK);

    memset(&shell, 0, sizeof(shell));
    shell.header.version = 2u;
    shell.header.schema_version = 4u;
    shell.header.logical_width = 65536u;
    shell.header.logical_height = 65536u;
    shell.header.sample_density = 1u;
    shell.header.layer_count = 1u;
    shell.header.next_layer_id = 2u;
    shell.header.raster_width = 65536u;
    shell.header.raster_height = 65536u;
    shell.header.raster_sample_count = 1u;
    shell.layers[0].layer_id = 1u;
    shell.layers[0].visible = 1u;

    assert(core_pack_writer_add_chunk(&w, "DPS3", &shell, sizeof(shell)).code == CORE_OK);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

static void write_sketch_pack_with_huge_object_count(const char *path) {
    CorePackWriter w = {0};
    DrawingSnapshotShellPrefixDisk shell;
    DrawingObjectChunkHeaderDisk object_hdr = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    assert(r.code == CORE_OK);

    memset(&shell, 0, sizeof(shell));
    shell.header.version = 2u;
    shell.header.schema_version = 4u;
    shell.header.logical_width = 4u;
    shell.header.logical_height = 4u;
    shell.header.sample_density = 1u;
    shell.header.layer_count = 1u;
    shell.header.next_layer_id = 2u;
    shell.header.raster_width = 4u;
    shell.header.raster_height = 4u;
    shell.header.raster_sample_count = 16u;
    shell.layers[0].layer_id = 1u;
    shell.layers[0].visible = 1u;

    object_hdr.version = 4u;
    object_hdr.object_count = UINT32_MAX;

    assert(core_pack_writer_add_chunk(&w, "DPS3", &shell, sizeof(shell)).code == CORE_OK);
    assert(core_pack_writer_add_chunk(&w, "DPOB", &object_hdr, sizeof(object_hdr)).code == CORE_OK);
    assert(core_pack_writer_close(&w).code == CORE_OK);
}

int main(void) {
    char fixture_dir[PATH_MAX];
    char physics_path[PATH_MAX];
    char daw_path[PATH_MAX];
    char trace_path[PATH_MAX];
    char sketch_path[PATH_MAX];
    char sketch_true_color_path[PATH_MAX];
    char huge_physics_path[PATH_MAX];
    char huge_daw_path[PATH_MAX];
    char huge_trace_path[PATH_MAX];
    char huge_manifest_path[PATH_MAX];
    char huge_sketch_raster_path[PATH_MAX];
    char huge_sketch_objects_path[PATH_MAX];

    make_test_temp_dir(fixture_dir, sizeof(fixture_dir));
    make_test_fixture_path(physics_path, sizeof(physics_path), fixture_dir, "unknown_physics.pack");
    make_test_fixture_path(daw_path, sizeof(daw_path), fixture_dir, "unknown_daw.pack");
    make_test_fixture_path(trace_path, sizeof(trace_path), fixture_dir, "unknown_trace.pack");
    make_test_fixture_path(sketch_path, sizeof(sketch_path), fixture_dir, "sketch_rects.pack");
    make_test_fixture_path(sketch_true_color_path, sizeof(sketch_true_color_path), fixture_dir, "sketch_true_color.pack");
    make_test_fixture_path(huge_physics_path, sizeof(huge_physics_path), fixture_dir, "huge_physics.pack");
    make_test_fixture_path(huge_daw_path, sizeof(huge_daw_path), fixture_dir, "huge_daw.pack");
    make_test_fixture_path(huge_trace_path, sizeof(huge_trace_path), fixture_dir, "huge_trace.pack");
    make_test_fixture_path(huge_manifest_path, sizeof(huge_manifest_path), fixture_dir, "huge_manifest.pack");
    make_test_fixture_path(huge_sketch_raster_path, sizeof(huge_sketch_raster_path), fixture_dir, "huge_sketch_raster.pack");
    make_test_fixture_path(huge_sketch_objects_path, sizeof(huge_sketch_objects_path), fixture_dir, "huge_sketch_objects.pack");

    write_physics_pack_with_unknown_chunk(physics_path);
    write_daw_pack_with_unknown_chunk(daw_path);
    write_trace_pack_with_unknown_chunk(trace_path);
    write_sketch_pack(sketch_path);
    write_sketch_true_color_pack(sketch_true_color_path);
    write_physics_pack_with_huge_grid(huge_physics_path);
    write_daw_pack_with_huge_point_count(huge_daw_path);
    write_trace_pack_with_huge_counts(huge_trace_path);
    write_daw_pack_with_huge_manifest(huge_manifest_path);
    write_sketch_pack_with_huge_raster_bounds(huge_sketch_raster_path);
    write_sketch_pack_with_huge_object_count(huge_sketch_objects_path);

    DatalabFrame physics = {0};
    CoreResult r = datalab_load_pack(physics_path, &physics);
    assert(r.code == CORE_OK);
    assert(physics.profile == DATALAB_PROFILE_PHYSICS);
    assert(physics.width == 2 && physics.height == 2);
    assert(physics.chunk_count == 5);
    assert(physics.density != NULL && physics.velx != NULL && physics.vely != NULL);
    datalab_frame_free(&physics);

    DatalabFrame daw = {0};
    r = datalab_load_pack(daw_path, &daw);
    assert(r.code == CORE_OK);
    assert(daw.profile == DATALAB_PROFILE_DAW);
    assert(daw.point_count == 4);
    assert(daw.marker_count == 2);
    assert(daw.wave_min != NULL && daw.wave_max != NULL && daw.markers != NULL);
    assert(daw.chunk_count == 6);
    datalab_frame_free(&daw);

    DatalabFrame trace = {0};
    r = datalab_load_pack(trace_path, &trace);
    assert(r.code == CORE_OK);
    assert(trace.profile == DATALAB_PROFILE_TRACE);
    assert(trace.trace_version == 1);
    assert(trace.trace_sample_count == 3);
    assert(trace.trace_marker_count == 1);
    assert(trace.trace_samples != NULL && trace.trace_markers != NULL);
    assert(trace.chunk_count == 4);
    datalab_frame_free(&trace);

    DatalabFrame sketch = {0};
    r = datalab_load_pack(sketch_path, &sketch);
    assert(r.code == CORE_OK);
    assert(sketch.profile == DATALAB_PROFILE_SKETCH);
    assert(sketch.width == 16u && sketch.height == 16u);
    assert(sketch.drawing_object_count == 2u);
    assert(sketch.drawing_rendered_object_count == 2u);
    assert(sketch.drawing_unsupported_object_count == 0u);
    assert(sketch.drawing_rgba != NULL);
    {
        size_t bg = ((size_t)0u * 16u + (size_t)0u) * 4u;
        size_t rect_a = ((size_t)4u * 16u + (size_t)3u) * 4u;
        size_t rect_b = ((size_t)9u * 16u + (size_t)10u) * 4u;
        assert(sketch.drawing_rgba[bg + 3u] == 0u);
        assert(sketch.drawing_rgba[rect_a + 0u] == 232u);
        assert(sketch.drawing_rgba[rect_a + 1u] == 232u);
        assert(sketch.drawing_rgba[rect_a + 2u] == 232u);
        assert(sketch.drawing_rgba[rect_a + 3u] == 255u);
        assert(sketch.drawing_rgba[rect_b + 0u] == 24u);
        assert(sketch.drawing_rgba[rect_b + 1u] == 24u);
        assert(sketch.drawing_rgba[rect_b + 2u] == 24u);
        assert(sketch.drawing_rgba[rect_b + 3u] == 255u);
    }
    datalab_frame_free(&sketch);

    DatalabFrame sketch_true_color = {0};
    r = datalab_load_pack(sketch_true_color_path, &sketch_true_color);
    assert(r.code == CORE_OK);
    assert(sketch_true_color.profile == DATALAB_PROFILE_SKETCH);
    assert(sketch_true_color.width == 4u && sketch_true_color.height == 4u);
    assert(sketch_true_color.drawing_schema_version == 4u);
    assert(sketch_true_color.drawing_layer_count == 2u);
    assert(sketch_true_color.drawing_object_count == 1u);
    assert(sketch_true_color.drawing_rendered_object_count == 1u);
    assert(sketch_true_color.drawing_unsupported_object_count == 0u);
    assert(sketch_true_color.drawing_rgba != NULL);
    {
        size_t overlay = ((size_t)0u * 4u + (size_t)0u) * 4u;
        size_t base = ((size_t)1u * 4u + (size_t)1u) * 4u;
        size_t object = ((size_t)2u * 4u + (size_t)2u) * 4u;
        assert(sketch_true_color.drawing_rgba[overlay + 0u] == 220u);
        assert(sketch_true_color.drawing_rgba[overlay + 1u] == 100u);
        assert(sketch_true_color.drawing_rgba[overlay + 2u] == 10u);
        assert(sketch_true_color.drawing_rgba[overlay + 3u] == 255u);
        assert(sketch_true_color.drawing_rgba[base + 0u] == 10u);
        assert(sketch_true_color.drawing_rgba[base + 1u] == 20u);
        assert(sketch_true_color.drawing_rgba[base + 2u] == 30u);
        assert(sketch_true_color.drawing_rgba[base + 3u] == 255u);
        assert(sketch_true_color.drawing_rgba[object + 0u] == 0u);
        assert(sketch_true_color.drawing_rgba[object + 1u] == 200u);
        assert(sketch_true_color.drawing_rgba[object + 2u] == 255u);
        assert(sketch_true_color.drawing_rgba[object + 3u] == 255u);
    }
    datalab_frame_free(&sketch_true_color);

    DatalabFrame hostile = {0};
    r = datalab_load_pack(huge_physics_path, &hostile);
    assert(r.code == CORE_ERR_FORMAT);
    datalab_frame_free(&hostile);

    r = datalab_load_pack(huge_daw_path, &hostile);
    assert(r.code == CORE_ERR_FORMAT);
    datalab_frame_free(&hostile);

    r = datalab_load_pack(huge_trace_path, &hostile);
    assert(r.code == CORE_ERR_FORMAT);
    datalab_frame_free(&hostile);

    r = datalab_load_pack(huge_manifest_path, &hostile);
    assert(r.code == CORE_ERR_FORMAT);
    datalab_frame_free(&hostile);

    r = datalab_load_pack(huge_sketch_raster_path, &hostile);
    assert(r.code == CORE_ERR_FORMAT);
    datalab_frame_free(&hostile);

    r = datalab_load_pack(huge_sketch_objects_path, &hostile);
    assert(r.code == CORE_ERR_FORMAT);
    datalab_frame_free(&hostile);

    cleanup_test_fixture(huge_sketch_objects_path);
    cleanup_test_fixture(huge_sketch_raster_path);
    cleanup_test_fixture(huge_manifest_path);
    cleanup_test_fixture(huge_trace_path);
    cleanup_test_fixture(huge_daw_path);
    cleanup_test_fixture(huge_physics_path);
    cleanup_test_fixture(sketch_true_color_path);
    cleanup_test_fixture(sketch_path);
    cleanup_test_fixture(trace_path);
    cleanup_test_fixture(daw_path);
    cleanup_test_fixture(physics_path);
    (void)rmdir(fixture_dir);

    puts("datalab pack loader test passed");
    return 0;
}

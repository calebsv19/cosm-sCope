#ifndef DATALAB_PACK_INSPECTOR_H
#define DATALAB_PACK_INSPECTOR_H

#include <stddef.h>
#include <stdint.h>

#include "core_base.h"

#define DATALAB_PACK_INSPECTION_MAX_CHUNKS 16u

typedef struct DatalabPackInspectionChunk {
    char type[5];
    uint64_t size;
} DatalabPackInspectionChunk;

typedef struct DatalabPackInspection {
    uint32_t version_major;
    uint32_t version_minor;
    size_t chunk_count;
    size_t listed_chunk_count;
    char family[48];
    DatalabPackInspectionChunk chunks[DATALAB_PACK_INSPECTION_MAX_CHUNKS];
} DatalabPackInspection;

CoreResult datalab_inspect_pack(const char *path, DatalabPackInspection *out_inspection);
void datalab_pack_inspection_format_summary(const DatalabPackInspection *inspection,
                                            char *out_text,
                                            size_t out_text_cap);
void datalab_pack_inspection_format_chunk(const DatalabPackInspection *inspection,
                                          size_t chunk_index,
                                          char *out_text,
                                          size_t out_text_cap);

#endif

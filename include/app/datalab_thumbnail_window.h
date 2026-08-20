#ifndef DATALAB_THUMBNAIL_WINDOW_H
#define DATALAB_THUMBNAIL_WINDOW_H

#include <stddef.h>
#include <stdint.h>

enum {
    DATALAB_THUMBNAIL_WINDOW_MAX_CANDIDATES = 8u
};

/*
 * Produces a bounded nearest-first prefetch order that favors the current
 * navigation direction. The selected index is never included.
 */
size_t datalab_thumbnail_window_indices(uint64_t selected,
                                        uint64_t item_count,
                                        int direction,
                                        uint64_t *out_indices,
                                        size_t out_capacity);

#endif

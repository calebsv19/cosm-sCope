#include "app/datalab_thumbnail_window.h"

static void datalab_thumbnail_window_add(uint64_t selected,
                                         uint64_t item_count,
                                         int64_t offset,
                                         uint64_t *out_indices,
                                         size_t out_capacity,
                                         size_t *io_count) {
    uint64_t index = 0u;
    if (!out_indices || !io_count || *io_count >= out_capacity || offset == 0) return;
    if (offset < 0) {
        const uint64_t distance = (uint64_t)(-offset);
        if (distance > selected) return;
        index = selected - distance;
    } else {
        const uint64_t distance = (uint64_t)offset;
        if (distance > UINT64_MAX - selected) return;
        index = selected + distance;
        if (index >= item_count) return;
    }
    out_indices[(*io_count)++] = index;
}

size_t datalab_thumbnail_window_indices(uint64_t selected,
                                        uint64_t item_count,
                                        int direction,
                                        uint64_t *out_indices,
                                        size_t out_capacity) {
    size_t count = 0u;
    size_t capacity = out_capacity;
    if (!out_indices || out_capacity == 0u || item_count == 0u || selected >= item_count) return 0u;
    if (capacity > DATALAB_THUMBNAIL_WINDOW_MAX_CANDIDATES) {
        capacity = DATALAB_THUMBNAIL_WINDOW_MAX_CANDIDATES;
    }
    if (direction > 0) {
        for (int64_t distance = 1; distance <= 6 && count < capacity; ++distance) {
            datalab_thumbnail_window_add(selected, item_count, distance, out_indices, capacity, &count);
        }
        for (int64_t distance = 1; distance <= 8 && count < capacity; ++distance) {
            datalab_thumbnail_window_add(selected, item_count, -distance, out_indices, capacity, &count);
        }
        for (int64_t distance = 7; distance <= 8 && count < capacity; ++distance) {
            datalab_thumbnail_window_add(selected, item_count, distance, out_indices, capacity, &count);
        }
    } else if (direction < 0) {
        for (int64_t distance = 1; distance <= 6 && count < capacity; ++distance) {
            datalab_thumbnail_window_add(selected, item_count, -distance, out_indices, capacity, &count);
        }
        for (int64_t distance = 1; distance <= 8 && count < capacity; ++distance) {
            datalab_thumbnail_window_add(selected, item_count, distance, out_indices, capacity, &count);
        }
        for (int64_t distance = 7; distance <= 8 && count < capacity; ++distance) {
            datalab_thumbnail_window_add(selected, item_count, -distance, out_indices, capacity, &count);
        }
    } else {
        for (int64_t distance = 1; distance <= 4 && count < capacity; ++distance) {
            datalab_thumbnail_window_add(selected, item_count, distance, out_indices, capacity, &count);
            datalab_thumbnail_window_add(selected, item_count, -distance, out_indices, capacity, &count);
        }
    }
    return count;
}

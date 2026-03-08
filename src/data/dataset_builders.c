#include "data/dataset_builders.h"

CoreResult datalab_build_dataset_from_frame(const DatalabFrame *frame, CoreDataset *out_dataset) {
    if (!frame || !out_dataset || frame->profile != DATALAB_PROFILE_PHYSICS ||
        !frame->density || !frame->velx || !frame->vely || frame->width == 0 || frame->height == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    core_dataset_init(out_dataset);

    CoreField2DDesc desc;
    desc.width = frame->width;
    desc.height = frame->height;
    desc.origin_x = frame->origin_x;
    desc.origin_y = frame->origin_y;
    desc.cell_size = frame->cell_size;

    CoreResult r = core_dataset_add_field2d_f32(out_dataset, "density", desc, frame->density);
    if (r.code != CORE_OK) return r;

    r = core_dataset_add_field2d_vec2f32(out_dataset, "velocity", desc, frame->velx, frame->vely);
    if (r.code != CORE_OK) {
        core_dataset_free(out_dataset);
        return r;
    }

    r = core_dataset_add_metadata_i64(out_dataset, "frame_index", (int64_t)frame->frame_index);
    if (r.code != CORE_OK) {
        core_dataset_free(out_dataset);
        return r;
    }

    r = core_dataset_add_metadata_f64(out_dataset, "time_seconds", frame->time_seconds);
    if (r.code != CORE_OK) {
        core_dataset_free(out_dataset);
        return r;
    }

    r = core_dataset_add_metadata_f64(out_dataset, "dt_seconds", frame->dt_seconds);
    if (r.code != CORE_OK) {
        core_dataset_free(out_dataset);
        return r;
    }

    return core_result_ok();
}

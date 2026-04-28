#ifndef DATALAB_PACK_LOADER_SKETCH_H
#define DATALAB_PACK_LOADER_SKETCH_H

#include "core_pack.h"
#include "data/pack_loader.h"

CoreResult datalab_pack_loader_load_sketch_profile(CorePackReader *reader,
                                                   const CorePackChunkInfo *dps3,
                                                   const CorePackChunkInfo *dps2,
                                                   DatalabFrame *out_frame);

#endif

#ifndef DATALAB_PACK_LOADER_INTERNAL_H
#define DATALAB_PACK_LOADER_INTERNAL_H

#include "core_pack.h"
#include "data/pack_loader.h"

CoreResult datalab_pack_loader_read_optional_manifest(CorePackReader *reader, DatalabFrame *out_frame);

#endif

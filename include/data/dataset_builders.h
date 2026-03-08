#ifndef DATALAB_DATASET_BUILDERS_H
#define DATALAB_DATASET_BUILDERS_H

#include "core_data.h"
#include "data/pack_loader.h"

CoreResult datalab_build_dataset_from_frame(const DatalabFrame *frame, CoreDataset *out_dataset);

#endif

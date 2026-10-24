#ifndef DUXCEL_OBJECTS_ASSET_INTERNAL_H
#define DUXCEL_OBJECTS_ASSET_INTERNAL_H
#include "objects_asset.h"
/* Private compound-writer boundary. Fresh DISJOINT caller-owned unpublished
   arena; validate complete model and scratch/capacity before writing. Input
   remains immutable through this single-owner call. Failure preserves written,
   but arena contents are unspecified: the compound owner discards/cancels them,
   never publishes a partial prefix. No intermediate packet/graph allocation;
   only bounded admitted grammar validation may allocate. Ordinary public packet
   capture retains its atomic independent-output replacement contract. */
ObjectsAssetStatus objects_asset_encode_unpublished(uint64_t sheet,const ObjectSnapshot *objects,uint64_t high_water,
    size_t validation_reserved,void *arena,size_t capacity,size_t *written);
#endif

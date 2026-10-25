#include "source_versions_admission.h"
#include <string.h>
SourceSubsetStatus source_versions_subset_admit(const SourceVersionsAsset *captured,
    uint64_t sheet,const SourceVersions *live,uint64_t owners,SourceSubsetAdmission *out) {
    if (!captured || !captured->sheet || !sheet || !out || !source_versions_valid(&captured->versions,UINT64_MAX,NULL) ||
        (live && !source_versions_valid(live,owners,NULL))) return SOURCE_SUBSET_INVALID;
    SourceSubsetAdmission result={0};
    for (size_t i=0;i<captured->versions.count;i++) {
        const SourceVersion *row=captured->versions.items+i;
        if (!row->raw.bytes) result.captured_pending++;
        const SourceVersion *current=source_versions_find(live,row->version);
        if (captured->sheet!=sheet) { result.conflicting++; continue; }
        if (current && (current->owner!=row->owner || current->kind!=row->kind || strcmp(current->path,row->path))) { result.conflicting++; continue; }
        if (!current || row->owner>owners || (row->raw.bytes && !current->raw.bytes)) { result.unavailable++; continue; }
        if (row->raw.bytes && (row->raw.bytes!=current->raw.bytes || memcmp(row->raw.data,current->raw.data,row->raw.bytes))) result.conflicting++;
        else if (!row->raw.bytes && current->raw.bytes) result.refined++;
        else result.exact++;
    }
    result.allocator_unavailable=!live || captured->versions.high_water>live->high_water;
    *out=result;
    if (captured->sheet!=sheet || result.conflicting) return SOURCE_SUBSET_CONFLICT;
    if (result.allocator_unavailable || result.unavailable) return SOURCE_SUBSET_UNAVAILABLE;
    return result.refined ? SOURCE_SUBSET_REFINED : SOURCE_SUBSET_EXACT;
}

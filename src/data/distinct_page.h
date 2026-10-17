#ifndef DUXCEL_DISTINCT_PAGE_H
#define DUXCEL_DISTINCT_PAGE_H
#include "page.h"
#define DX_DISTINCT_PAGE_BYTES (4u*1024u*1024u)
/* Convert the shared query's three VARCHAR columns value/label/upper_bound.
   Copies all strings with the C allocator. NULL has label "NULL" and no key;
   non-NULL empty/literal text remains distinct. A date key is value+'\t'+upper.
   exact_total is the matching count reply or the caller's scoped known count.
   Input is <=128 rows; all output allocations count toward 4 MiB. out must be
   zero-initialized or owned; failure preserves it. No wire/Page memory retained. */
bool distinct_page_from_page(DistinctPage *out,const Page *values,uint64_t exact_total,char **error);
#endif

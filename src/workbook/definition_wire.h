#ifndef DUXCEL_DEFINITION_WIRE_H
#define DUXCEL_DEFINITION_WIRE_H
#include "../transforms/analysis.h"
#define DX_DEFINITION_OWNED_BYTES (8u*1024u*1024u)
/* Internal shared definition-codec primitives. Family headers/tags/semantics
   remain with their typed owners. Fresh writers have already measured disjoint
   capacity; integer widths are in1..8. Readers fail closed without advancing
   beyond input. Text lengths distinguish NULL from empty; UTF-8 forbids NUL. */
typedef struct { size_t bytes,owned,validation_scratch; } DefinitionWireMeasure;
typedef struct { unsigned char *p; } DefinitionWireWriter;
typedef struct { const unsigned char *data; size_t bytes,at,owned,scratch,owned_limit; bool ok; } DefinitionWireReader;
bool definition_wire_text_measure(const char *text,DefinitionWireMeasure *measure,size_t owned_limit,size_t *length);
void definition_wire_put(DefinitionWireWriter *writer,uint64_t value,unsigned bytes);
void definition_wire_text_write(DefinitionWireWriter *writer,const char *text);
uint64_t definition_wire_get(DefinitionWireReader *reader,unsigned bytes);
/* Optional out copies only on the SECOND pass, after complete preflight. The
   first pass uses NULL and performs no allocation. false+ok means a NULL span;
   false+!ok is invalid framing/text/budget. Owned totals include terminators. */
bool definition_wire_text_read(DefinitionWireReader *reader,char **out,size_t *length);
/* Existing nonrecursive BETWEEN/COMPOSITE parser's conservative logical peak,
   excluding allocator/RSS/errors. n is bounded by the definition owned limit;
   SIZE_MAX rejects a larger input. Call before any grammar parsing/allocation. */
size_t definition_wire_filter_scratch(FilterOp op,size_t bytes,bool present);
#endif

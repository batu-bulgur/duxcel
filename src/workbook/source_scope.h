#ifndef DUXCEL_SOURCE_SCOPE_H
#define DUXCEL_SOURCE_SCOPE_H
#include "../data/page.h"
#include "../data/source_digest.h"

#define DX_SOURCE_SCOPE_BYTES (256u*1024u) /* Packet bytes must be strictly smaller. */
#define DX_SOURCE_SCOPE_FIELDS 16384u
#define DX_SOURCE_SCOPE_NAME_BYTES 4096u
#define DX_SOURCE_SCOPE_TEXT_BYTES 65536u
#define DX_SOURCE_SCOPE_ENGINE_BYTES 128u
#define DX_SOURCE_VERSIONS 1024u
#define DX_SOURCE_VERSIONS_BYTES (1024u*1024u)

typedef struct { unsigned char *data; size_t bytes; } SourcePacket;
typedef enum { SOURCE_SCOPE_RAW=1,SOURCE_SCOPE_RESULT=2 } SourceScopeKind;
typedef enum { SOURCE_READER_CSV_ORDINAL_V1=1,SOURCE_READER_PARQUET_ORDINAL_V1=2 } SourceReaderPolicy;
/* Borrowed views into a validated packet. No view owns storage or engine handles. */
typedef struct {
    uint32_t ordinal;
    const char *name;
    duckdb_type type;
    NumberType number;
} SourceScopeField;
typedef struct {
    SourceScopeKind kind;
    SourceKind source_kind;
    SourceReaderPolicy reader;
    uint64_t owner,version,file_bytes;
    const char *engine_version;
    const unsigned char *sha256,*raw,*fields;
    size_t raw_bytes,field_count,field_bytes;
} SourceScopeInfo;

/* These are codecs, not filesystem/content verification. Callers bind a successful
   digest to the captured Page/request/source lifetime before calling. raw_build
   borrows only the original complete Column schema, never cell display values.
   CSV/Parquet only; query/DISTINCT/group and incomplete/nested types fail closed.
   The raw encoding excludes filters/sorts, path spelling, file stat identity,
   cache stamps and presentation. SourceVersion.path supplies the locator fence.
   Result encoding adds canonical original-field filters and ordered sorts; it
   does not infer lineage or accept arbitrary SQL. Explicit NULL/empty/literal
   filter values remain distinct. Engine/reader compatibility is exact.
   Every active filter must carry its exact schema-resolved NumberType (including
   precision/scale); nonnumeric fields require canonical zero. Derived bindings
   are validated, never serialized independently. Unresolved numeric filters
   fail closed, including in raw_build even though raw identity omits filters.
   out is zero-initialized or owned; every failure preserves it. */
bool source_scope_raw_build(uint64_t owner,uint64_t version,const Analysis *source,
    const Page *schema,const SourceDigest *verified,const char *engine_version,
    SourcePacket *out,char **error);
bool source_scope_result_build(const SourcePacket *raw,const Analysis *analysis,
    SourcePacket *out,char **error);
bool source_scope_validate(const void *data,size_t bytes,SourceScopeInfo *out,char **error);
bool source_scope_field(const void *data,size_t bytes,size_t ordinal,SourceScopeField *out,char **error);
bool source_scope_schema_matches(const void *data,size_t bytes,const Page *schema,char **error);
void source_packet_free(SourcePacket *packet);

typedef struct {
    uint64_t owner,version;
    SourceKind kind;
    char *path;              /* Owned locator, resolved by the workbook codec. */
    SourcePacket raw;        /* Empty=pending; complete raw proof is immutable. */
} SourceVersion;
typedef struct {
    uint64_t high_water;     /* Last issued; deliberately outside Undo. */
    SourceVersion *items;
    size_t count,capacity,bytes; /* bytes is exact owned arrays/text/packet bytes. */
} SourceVersions;

/* The caller must use the actual worksheet object high-water at its persistence
   boundary. Structural copy/plan do not claim a live owner or verified file.
   Pending entries cannot match scopes. A complete packet must exactly identify
   its entry's owner/version/kind; a result must embed that exact raw packet.
   scope_matches also validates the current Analysis locator and transforms.
   These command/publication helpers are not paint/cell-loop callbacks. */
bool source_versions_valid(const SourceVersions *versions,uint64_t object_high_water,char **error);
bool source_versions_copy(SourceVersions *out,const SourceVersions *from,char **error);
void source_versions_free(SourceVersions *versions);
/* issue=true requires entry.version==current.high_water+1, with no wrap/reuse.
   issue=false completes one pending entry exactly once, preserving its locator.
   Preflights the next <=1 MiB ledger before allocating it. current is untouched;
   out must be distinct and zero-initialized or owned. Input and next ownership
   coexist during planning; this bound is not allocator/process peak accounting. */
bool source_versions_plan(const SourceVersions *current,const SourceVersion *entry,
    bool issue,SourceVersions *out,char **error);
bool source_versions_scope_matches(const SourceVersions *versions,uint64_t owner,uint64_t version,
    const Analysis *current,const void *scope,size_t bytes,char **error);
#endif

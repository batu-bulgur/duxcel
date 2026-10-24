#ifndef DUXCEL_WORKBOOK_VERSION_INDEX_H
#define DUXCEL_WORKBOOK_VERSION_INDEX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DX_VERSION_KEY_BYTES 32u
#define DX_VERSION_ROOTS 1024u
#define DX_VERSION_TAGS 1024u
#define DX_VERSION_LABEL_BYTES 256u
/* Opaque identity of a complete payload. Every byte pattern is allowed, including
   all zeroes. Optional references use presence, never a special key value.
   The native adapter must independently prove encoding, integrity, document
   association and complete asset closure before publishing these descriptors. */
typedef struct { unsigned char bytes[DX_VERSION_KEY_BYTES]; } WorkbookVersionKey;
typedef struct { bool present; WorkbookVersionKey key; } WorkbookVersionRef;
typedef struct {
    WorkbookVersionKey key;
    uint64_t bytes;
    uint32_t encoding;
} WorkbookVersionRoot;
typedef struct { const char *data; size_t bytes; } WorkbookVersionText;
typedef struct {
    WorkbookVersionText label; /* Borrowed exact UTF-8; no case folding/normalization. */
    WorkbookVersionKey root;
    uint64_t created; /* Index publication generation, never wall time. */
} WorkbookVersionTag;
typedef struct {
    uint64_t document_high,document_low,generation,saved_generation;
    WorkbookVersionRef saved;
    const WorkbookVersionRoot *roots;
    size_t root_count;
    const WorkbookVersionTag *tags;
    size_t tag_count;
} WorkbookVersionIndex;
typedef enum { VERSION_WORKING_HEAD,VERSION_HISTORICAL_VIEW } WorkbookVersionMode;
typedef struct {
    uint64_t document_high,document_low,session;
    WorkbookVersionMode mode;
} WorkbookVersionOwner;
typedef enum { VERSION_USER_SAVE,VERSION_RECOVERY_CHECKPOINT,VERSION_STATE_EXPORT } WorkbookVersionPurpose;
typedef enum {
    VERSION_OK,VERSION_INVALID_INDEX,VERSION_INVALID_ROOT,VERSION_INVALID_LABEL,
    VERSION_DUPLICATE_TAG,VERSION_LIMIT,VERSION_EXHAUSTED,VERSION_READ_ONLY,
    VERSION_INVALID_OWNER,VERSION_INVALID_PURPOSE,VERSION_ROOT_CONFLICT
} WorkbookVersionStatus;
typedef struct {
    WorkbookVersionOwner owner;
    WorkbookVersionPurpose purpose;
    uint64_t generation,next_generation,saved_generation,next_saved_generation;
    size_t root_count,tag_count;
    WorkbookVersionRef before_saved,after_saved;
    WorkbookVersionRoot root;
    bool adds_root,updates_saved;
} WorkbookVersionPlan;
/* Pure bounded structural/chronology checks. Arrays/spans are borrowed and
   readable for the call. This is not a payload decoder or a storage budget.
   Limits reject before publication; they never evict retained states or tags.
   Root order is immaterial. Tag labels and creation generations are unique.
   A published nonempty index always has a resolvable Latest Saved reference. */
WorkbookVersionStatus workbook_version_index_check(const WorkbookVersionIndex *index);
/* Produce an owned value plan without changing index/out on failure. User Save
   takes an independently verified complete root descriptor. Repeated identical
   Save reuses its root and generation. Checkpoint/export require root=NULL and
   preserve the user index, including an older saved state in an R+1 checkpoint.
   A historical owner can only export. The owner session is transient and must
   distinguish replaced write owners even when durable document IDs match. */
WorkbookVersionStatus workbook_version_prepare(const WorkbookVersionIndex *index,
    WorkbookVersionOwner owner,WorkbookVersionPurpose purpose,
    const WorkbookVersionRoot *root,WorkbookVersionPlan *out);
/* Published index metadata is immutable between generation changes. This
   in-process plan is not a wire record. The adapter retains its complete owned
   candidate/assets before IO, then checks the original write owner/index after
   successful publication. Working data may have advanced to R+1 independently;
   the App still applies its separate dirty/Save fence. No allocation or IO. */
bool workbook_version_can_publish(const WorkbookVersionIndex *index,
    WorkbookVersionOwner owner,const WorkbookVersionPlan *plan);
const char *workbook_version_message(WorkbookVersionStatus status);
#endif

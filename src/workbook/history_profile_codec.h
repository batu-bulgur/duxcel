#ifndef DUXCEL_HISTORY_PROFILE_CODEC_H
#define DUXCEL_HISTORY_PROFILE_CODEC_H
#include "history_profile.h"
#define DX_HISTORY_PROFILE_ENCODING 1u
#define DX_HISTORY_PROFILE_HEADER_BYTES 160u
#define DX_HISTORY_PROFILE_PARSE_BYTES (4u*1024u*1024u)
#define DX_HISTORY_PROFILE_WIRE_BYTES (DX_HISTORY_PROFILE_OWNED_BYTES+4u*1024u*1024u)
typedef struct { size_t bytes,parse_bytes,alignment,records; } WorkbookHistoryProfileWireInfo;
typedef struct { WorkbookVersionOwner owner; uint64_t edit_session; } WorkbookHistoryProfileBinding;
typedef enum { HISTORY_PROFILE_CODEC_OK,HISTORY_PROFILE_CODEC_INVALID,HISTORY_PROFILE_CODEC_UNSUPPORTED,
    HISTORY_PROFILE_CODEC_LIMIT,HISTORY_PROFILE_CODEC_ADMISSION } WorkbookHistoryProfileCodecStatus;
/* Unpublished CURRENT DXHP encoding1 component framing, not an activated dxls
   schema or file profile. Explicit checkpoint-only/advertised kind and current
   native16/core15 policy are encoded. No historical fallback/default/upgrade.
   Canonical metadata ordering does not change path order, unique transition
   order, slot association or reusable action identity. Existing typed payload
   bytes are embedded exactly; their existing semantic decoders remain sole
   authorities. No process pointer, owner session/mode or edit-session is stored.
   Runtime binding must name the exact wire document with fresh nonzero fences;
   it never overrides durable document/target/path/effect/payload identities.
   Structural preflight allocates nothing and preserves info on failure. It
   checks bounded counts/spans/reserved values/order before any typed writes.
   The parse arena must be independently caller-owned, disjoint from wire and
   any candidate/input, and aligned to info.alignment (C99 malloc/dx_alloc meets
   the contract). decode charges its entire supplied arena capacity plus the
   exact candidate/prior/maximum serial semantic peak against caps.staging_bytes
   before candidate allocation. Arena contents are disposable on every outcome;
   metadata strings/payloads borrow wire until existing admission copies them.
   Existing wire/input residency remains separately charged by its caller.
   Success owns no parser arena or wire bytes. Failure preserves the old output.
   encode requires an independent writable byte arena; all bounds are checked
   before writes. No native physical root proof, IO/publication, live adoption,
   execution readiness or retained traversal is implied. Native handoff/engine
   acceptance is separate from this codec's pure checks. */
WorkbookHistoryProfileCodecStatus workbook_history_profile_encoded_size(const WorkbookHistoryProfile *profile,size_t *bytes);
WorkbookHistoryProfileCodecStatus workbook_history_profile_encode(const WorkbookHistoryProfile *profile,void *arena,size_t capacity,size_t *written);
WorkbookHistoryProfileCodecStatus workbook_history_profile_preflight(const void *data,size_t bytes,WorkbookHistoryProfileWireInfo *out);
WorkbookHistoryProfileCodecStatus workbook_history_profile_decode(const void *data,size_t bytes,WorkbookHistoryProfileBinding binding,
    WorkbookHistoryProfileCaps caps,void *arena,size_t arena_bytes,WorkbookHistoryProfile **out);
#endif

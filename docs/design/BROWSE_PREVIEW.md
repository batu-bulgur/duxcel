# Native connected browse previews

This is the active W15 handoff contract for an optional bounded browse cache. It does not establish retained inputs, offline query execution, complete analytical output, W16 Import, W17 Data Model or W21 result retention. The current native container v16 and embedded core v15 remain separate current layers; the pure preview component does not install a new workbook schema or writer.

## Current ownership and admission

`src/workbook/browse_preview.h` owns one independent immutable browse interval. Capture resolves an actual current primary or Table result through the borrowed typed resolver seam and checks its document/sheet owner, actual Table definition, request/query/source generations, metadata clock, selected source binding and Page descriptors. A pending count is eligible when the interval itself is ready; its total remains an observed lower bound. The complete-result MOVE resolver is unchanged.

The new owner is bounded to 256 rows, 256 columns, 4 MiB of cell text and 8 MiB of total new ownership. Values are not individually truncated. Limits or unavailable/stale capture preserve the prior owner; an outer producer can omit an uncertified or oversized optional preview. Admission charges prior/new ownership, captured target, temporary definition grammar/RAW validation and separately accounted live inputs. These are admitted ownership limits, not an RSS guarantee.

Columns, exact nullable cell text and display-only `Cell.number` bits are independent C allocations. SQL NULL, zero-length text and literal `NULL` remain distinct. Signed zero, NaN and infinity are not normalized. Actual PageRecords prefixes may cover fewer rows than the interval; absent IDs remain absent. Reader keys, stamps and file observations are observations, not persistent record identity or permission to activate source formats. Runtime SQL/catalog/timing and analytical totals/Other are not retained browse assets.

Canonical DXAN definition and selected current DXSV metadata provide inert provenance. An unbound source version `0` selects no ledger rows; its captured high-water remains an allocator observation. Complete RAW metadata is copied independently, without source rows or a second source authority. Strict definition/owner/source association reuses the existing SourceSubset reports. Pending metadata can refine while retaining `captured_pending`; it never becomes execution or formatting permission. A changed source version, including `0` to a bound version, must not be silently promoted or remapped.

`src/workbook/browse_preview_codec.h` admits only current DXBP encoding 1. Its bounded little-endian framing contains the existing DXAN/DXSV children, typed browse data and record observations. Full child grammar and association admission is shared. It contains no pointers, native structs, legacy decoder or migration. Native physical root/hash and outer artifact association remain separate requirements.

The actual-Workbook association boundary resolves the stable sheet and primary owner `0` or real Table from one already-admitted immutable model. An archive reader must pass its already-loaded unpublished Workbook, rather than decode the database again or fabricate expected Analysis/ledger values. Runtime session/request/query/source epochs after load are observations, not durable freshness identities. Exact/refined association classifies only a captured preview candidate.

## Mandatory preview-only use

Restored browse data remains a separate preview-only read owner and renderer/readback state. It must never be installed as a completed App/ObjectView Page reply or set live result readiness. Existing format, move, manual-cell, query and current-statistics authorities continue to use the actual result owner. Metadata EXACT/REFINED does not prove current data, RAW verification, source availability or record-format permission.

Future cached copy/export must explicitly select the captured interval and its provenance. It must not implicitly execute or export the current external relation. Uncached coverage is unavailable, not fabricated empty cells or SQL NULL. A failed or later external query must not make an old preview appear current. Source availability continues to describe the actual selected connection.

## First native persistence scope

The first persistence slice is an **auxiliary preview set for the actual current artifact publication head**. It is not an immutable historical-version output. Do not show this set in named historical previews or borrow Working Head cache data for historical Save As. Those uses require separate original attachment/provenance work.

A later save may change the auxiliary cache while reusing the same semantically equal core root. Each committed artifact's cache assets remain immutable. This is optional current-document browse cache, not a promise of data time travel or analytical result retention.

Binding must use the actual purpose-specific publication target. User Save, Tag and recovery intents may target different roots; Latest Saved is not automatically the authored head. A newly sealed capture's physical SHA256 is not automatically the final Saved-root key: existing-root reuse requires the actual selected target and the existing complete-core comparison receipt. DXAN/DXSV metadata compatibility does not replace exact physical root/outer attachment proof.

The pure PreviewSet owns at most one immutable asset per stable `(sheet, source owner)` in one actual snapshot. Its manifest is ordered by that tuple and records actual document/revision plus captured-only metadata admission. Named existing sheet/object limits bound the inventory; the aggregate retained quota is explicit. Full new/prior wrapper, exact entry capacity, asset and temporary-admission coexistence is charged. Shared prior/new payloads may be conservatively charged twice.

Payload and set references use portable mutex-protected lifetime counters with checked overflow. Callers explicitly retain borrowed immutable entries before supplying owned references. Atomic take consumes no input on cancellation, duplicate keys, incompatibility or budget refusal; success nulls transferred slots. This permits offline/inactive-sheet cache reuse without payload clones or stealing the renderer's ownership. It does not create source/model authority or a publication receipt. Worksheet/schema/App Save installation, native attachment and offline display remain separate integration children. The current source-details dialogs and truthful Connect labels do not claim that preview persistence is already active.

## Closed PreviewSet component

`browse_preview_set_codec.h` defines current DXPS encoding 1, containing a fixed 64-byte little-endian header, a fixed 32-byte directory entry per owner and contiguous current DXBP children. The header declares auxiliary current-artifact scope and the exact document and persisted snapshot revision. Directory keys are strictly ascending unique `(sheet, owner)` pairs; child lengths, child keys/documents, reserved zero fields and terminal length must all agree. Unknown encodings/scopes, missing or unknown children and trailing bytes reject without defaulting.

Read measurement inspects framing without allocation and charges the complete new Set/asset owners, prior Set, borrowed input bytes, exact parser pointer capacity, maximum sequential child validation scratch and caller-supplied other ownership. The retained quota is explicit and independent of that coexistence reservation. Decode privately stages typed DXBP storage, then delegates the single semantic DXAN/DXSV and actual-Workbook association per entry to atomic Set take. Cancellation, pressure or a final incompatible child releases all staging and preserves the prior Set. Empty sets are explicit current components. Full existing sheet/object inventory bounds apply; the 2,112-owner case is covered in release and sanitizer tests.

This component is deliberately below native publication: it neither selects a Saved/Tag/recovery root nor checks artifact body hashes or publication receipts. The native stage adapter must supply the already admitted unpublished actual model and independently prove the purpose-specific current publication target. No runtime Page readiness, historical attachment or writer activation follows from component decoding.

## Native User Save staging boundary

The separate `browse_preview_component.h` adapter consumes only a genuine prepared User Save publication. The controller's active callback supplies the exact borrowed transaction, actual after-CoreSet and immutable captured Workbook; its snapshot getter rejects a wrong connection, a different after owner or an inactive phase. The adapter resolves and checks existing PreviewSet entries against that actual snapshot, without another database/model decoder. Tag, checkpoint and all export intents remain unsupported by this first adapter.

The fresh `_duxcel_previews` component records the actual selected physical target bytes/hash, captured document/revision and exact DXPS bytes/hash. A reused target is accepted only through the controller's genuine complete-core comparison/selection. Ordered bounded chunks are read back byte-for-byte within the same transaction, with capture/selected leases and payload hash rechecked before success. New payload storage and full live/packet/admission/chunk coexistence grants are explicit; they are not engine RSS guarantees.

The adapter never owns the connection, begins/commits/rolls back a transaction, advertises a registry bit or installs App cache/readiness. A non-ready result requires outer rollback. Its scalar ready result is prepublication evidence, not a committed receipt or permission to borrow cache for historical views. Normal native registry/Open/overwrite integration and actual engine acceptance remain separate; the focused native fixture is registered for root's serial engine window.

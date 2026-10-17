#ifndef DUXCEL_REVIEW_REGISTRY_H
#define DUXCEL_REVIEW_REGISTRY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Portable review-only records. The production build must not link this unit.
   Coordinates are window logical pixels; transform maps widget-local coordinates
   into that space. No OS, renderer, database or borrowed result pointers. */
#define DX_REVIEW_WIDGETS 2048
#define DX_REVIEW_JSON_LIMIT (4u*1024u*1024u)
#define DX_REVIEW_EVENTS 128
#define DX_REVIEW_NOTE_BYTES 4096

typedef struct { float x,y,width,height; } DxReviewRect;
typedef struct { float a,b,c,d,tx,ty; } DxReviewTransform;
typedef enum {
    DX_REVIEW_OK,DX_REVIEW_NOT_ARMED,DX_REVIEW_SEALED,DX_REVIEW_INVALID,
    DX_REVIEW_DUPLICATE,DX_REVIEW_CAPACITY,DX_REVIEW_NOT_SEALED,
    DX_REVIEW_JSON_TOO_LARGE,DX_REVIEW_BUFFER_TOO_SMALL,DX_REVIEW_NO_MEMORY
} DxReviewStatus;
typedef struct {
    bool present,has_cell;
    char kind[32],document_id[128],field[128];
    uint64_t sheet_id,source_id,object_id,schema_generation,query_generation,row,column;
} DxReviewDataIdentity;
typedef struct {
    bool present,is_null,truncated;
    char type[64],exact_text[1024]; /* SQL type and original prefix, never double. */
} DxReviewValue;
typedef struct {
    char id[128],action_id[96],path[192],parent_id[128],role[48],label[192];
    char source_file[192]; uint32_t source_line;
    DxReviewRect bounds,clip; DxReviewTransform transform;
    int32_t layer;
    bool enabled,input_eligible,checked,indeterminate,expanded,focused,container,pickable,truncated,identity_truncated;
    DxReviewDataIdentity data;
    DxReviewValue value;
} DxReviewWidget;
typedef struct {
    uint64_t frame_id,timestamp_ns;
    char session_id[96],project_id[96],document_id[128];
    char source_fingerprint[96],binary_sha256[65],build_flags[512],assets_fingerprint[96];
    char fonts[256],state_text[4096],trace_text[16384];
    uint32_t window_width,window_height,framebuffer_width,framebuffer_height;
    float dpi_x,dpi_y,ui_scale,grid_zoom;
    bool truncated;
} DxReviewFrame;
typedef enum { DX_REVIEW_PICK_ANY,DX_REVIEW_PICK_CONTROL,DX_REVIEW_PICK_CONTAINER } DxReviewPick;
typedef struct {
    bool found,region;
    uint64_t frame_id;
    DxReviewRect bounds;
    DxReviewWidget widget; /* Owned frozen record; valid after registry reuse. */
} DxReviewTarget;
typedef struct {
    DxReviewFrame frame;
    DxReviewWidget widgets[DX_REVIEW_WIDGETS];
    size_t count,duplicate_count,overflow_count,invalid_count;
    bool armed,sealed,truncated;
} DxReviewRegistry; /* Allocate once on the owning App heap, not its stack. */

/* begin never allocates or clears the 2048-slot storage. A false arm value needs
   no frame and discards the previous snapshot. After seal, all records are frozen
   until begin is called again. Instrumentation must not evaluate arguments when
   the compile-time review feature is absent. */
DxReviewStatus dx_review_begin(DxReviewRegistry *registry,const DxReviewFrame *frame,bool armed);
void dx_review_widget_init(DxReviewWidget *widget,const char *id,const char *action_id,
    const char *path,const char *role,const char *label);
/* A NULL exact_text pointer without is_null means no captured value. */
void dx_review_value_set(DxReviewValue *value,const char *sql_type,bool is_null,const char *exact_text);
DxReviewStatus dx_review_add(DxReviewRegistry *registry,const DxReviewWidget *widget);
DxReviewStatus dx_review_seal(DxReviewRegistry *registry);
const DxReviewWidget *dx_review_find(const DxReviewRegistry *registry,const char *widget_id); /* Borrowed registry record. */
bool dx_review_pick(const DxReviewRegistry *registry,float x,float y,DxReviewPick mode,DxReviewTarget *out);
bool dx_review_parent(const DxReviewRegistry *registry,const DxReviewTarget *child,DxReviewTarget *out);
bool dx_review_region(const DxReviewRegistry *registry,DxReviewRect region,DxReviewTarget *out);
/* A data target's ordinal is meaningful only in exactly this document/source/
   object/schema/query scope. Compare against the corresponding live scope, not
   merely the current active sheet. Non-data targets are never query-stale. */
bool dx_review_target_stale(const DxReviewTarget *target,const DxReviewDataIdentity *current);

/* IDs here (report/session/project) are ASCII [A-Za-z0-9_-], 1..95 bytes.
   Top-level schema="duxcel.native-review",version=1,report_id,session_id,
   project_id,frame,target,widgets,events,note. uint64 values are decimal strings;
   state_text/trace events are explicitly text, never fabricated structured state.
   JSON owns no registry pointers. Only sealed, armed snapshots serialize.
   Caller-buffer output is empty on failure; required includes trailing NUL.
   allocated output uses malloc/free and is capped at DX_REVIEW_JSON_LIMIT. */
DxReviewStatus dx_review_json(const DxReviewRegistry *registry,const DxReviewTarget *target,
    const char *report_id,const char *note,char *buffer,size_t capacity,size_t *required);
char *dx_review_json_alloc(const DxReviewRegistry *registry,const DxReviewTarget *target,
    const char *report_id,const char *note,size_t *length,DxReviewStatus *status);
const char *dx_review_status_text(DxReviewStatus status);
#endif

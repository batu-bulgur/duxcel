#include "popup.h"
void ui_popup_presence(UIPopupPresence *p,bool active,double now,bool immediate) {
    if (!p) return;
    if (!p->initialized) {
        p->initialized=true;
        p->opacity.ease_out=true;
        ui_motion_update_at(&p->opacity,0,now,.110f,true);
        ui_motion_update_at(&p->translation,-4,now,.120f,true);
    }
    /* A new popup after a completed exit starts from the definition's -4px.
       Reopening during exit reverses from the current paint without a jump. */
    if (active && !p->active && !p->visible) {
        ui_motion_update_at(&p->opacity,0,now,.110f,true);
        ui_motion_update_at(&p->translation,-4,now,.120f,true);
    }
    p->active=active;
    p->alpha=ui_motion_update_at(&p->opacity,active ? 1 : 0,now,.110f,immediate);
    p->offset=ui_motion_update_at(&p->translation,active ? 0 : -3,now,.120f,immediate);
    p->visible=active || p->alpha>0;
}

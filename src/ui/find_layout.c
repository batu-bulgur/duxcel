#include "find_layout.h"
#include <math.h>
UIFindLayout ui_find_layout(Rectangle area,float scale) {
    UIFindLayout l={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(area.x) || !isfinite(area.y) || !isfinite(area.width) || !isfinite(area.height) ||
        !isfinite(area.x+area.width) || !isfinite(area.y+area.height) || area.width/scale<296 || area.height/scale<252) return l;
    float width=fminf(420,area.width/scale-16),height=236,x=area.x+(area.width-width*scale)/2,y=area.y+(area.height-height*scale)/2;
    l.panel=(Rectangle){x,y,width*scale,height*scale}; l.title=(Rectangle){x+14*scale,y+9*scale,(width-28)*scale,24*scale};
    l.label=(Rectangle){x+14*scale,y+40*scale,(width-28)*scale,20*scale};
    l.input=(Rectangle){x+14*scale,y+62*scale,(width-28)*scale,28*scale};
    l.whole_cell=(Rectangle){x+14*scale,y+100*scale,(width-28)*scale,24*scale};
    l.message=(Rectangle){x+14*scale,y+134*scale,(width-28)*scale,42*scale};
    l.close=(Rectangle){x+14*scale,y+194*scale,70*scale,28*scale};
    l.previous=(Rectangle){x+(width-188)*scale,y+194*scale,84*scale,28*scale};
    l.next=(Rectangle){x+(width-96)*scale,y+194*scale,82*scale,28*scale}; l.valid=true; return l;
}

#include "go_to_layout.h"
#include <math.h>
UIGoToLayout ui_go_to_layout(Rectangle area,float scale) {
    UIGoToLayout l={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(area.x) || !isfinite(area.y) || !isfinite(area.width) || !isfinite(area.height) ||
        !isfinite(area.x+area.width) || !isfinite(area.y+area.height) || area.width/scale<296 || area.height/scale<204) return l;
    float width=fminf(360,area.width/scale-16),height=188,x=area.x+(area.width-width*scale)/2,y=area.y+(area.height-height*scale)/2;
    l.panel=(Rectangle){x,y,width*scale,height*scale}; l.title=(Rectangle){x+14*scale,y+9*scale,(width-28)*scale,24*scale};
    l.label=(Rectangle){x+14*scale,y+40*scale,(width-28)*scale,20*scale};
    l.input=(Rectangle){x+14*scale,y+62*scale,(width-28)*scale,28*scale};
    l.message=(Rectangle){x+14*scale,y+98*scale,(width-28)*scale,30*scale};
    l.cancel=(Rectangle){x+(width-184)*scale,y+146*scale,80*scale,28*scale};
    l.go=(Rectangle){x+(width-96)*scale,y+146*scale,82*scale,28*scale}; l.valid=true; return l;
}

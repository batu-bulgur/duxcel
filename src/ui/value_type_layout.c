#include "value_type_view.h"
#include <math.h>
UIValueTypeLayout ui_value_type_layout(Rectangle viewport,float scale) {
    UIValueTypeLayout l={0};
    if (!isfinite(scale) || scale<=0 || !isfinite(viewport.x) || !isfinite(viewport.y) || !isfinite(viewport.width) || !isfinite(viewport.height) ||
        !isfinite(viewport.x+viewport.width) || !isfinite(viewport.y+viewport.height) || viewport.width/scale<296 || viewport.height/scale<280) return l;
    float width=fminf(360,viewport.width/scale-16),height=264,x=viewport.x+(viewport.width-width*scale)/2,y=viewport.y+(viewport.height-height*scale)/2;
    l.panel=(Rectangle){x,y,width*scale,height*scale}; l.title=(Rectangle){x+12*scale,y+8*scale,(width-24)*scale,24*scale};
    float inner=width-24,cell=(inner-12)/3;
    for (unsigned i=0;i<3;i++) l.family[i]=(Rectangle){x+(12+i*(cell+6))*scale,y+40*scale,cell*scale,26*scale};
    l.family[UI_VALUE_TYPE_DECIMAL]=(Rectangle){x+12*scale,y+70*scale,(cell*2+6)*scale,26*scale};
    l.family[UI_VALUE_TYPE_CLEAR]=(Rectangle){x+(24+cell*2)*scale,y+70*scale,cell*scale,26*scale};
    for (unsigned i=0;i<2;i++) {
        l.details[i]=(Rectangle){x+12*scale,y+(108+i*34)*scale,inner*scale,26*scale};
        l.sign[i]=(Rectangle){x+(12+i*(inner+6)/2)*scale,y+108*scale,(inner-6)/2*scale,26*scale};
        l.minus[i]=(Rectangle){x+108*scale,l.details[i].y,30*scale,26*scale};
        l.value[i]=(Rectangle){x+144*scale,l.details[i].y,72*scale,26*scale};
        l.plus[i]=(Rectangle){x+222*scale,l.details[i].y,30*scale,26*scale};
    }
    for (unsigned i=0;i<5;i++) l.bits[i]=(Rectangle){x+(12+i*(inner+6)/5)*scale,y+142*scale,(inner-24)/5*scale,26*scale};
    l.help=(Rectangle){x+12*scale,y+180*scale,inner*scale,32*scale};
    l.cancel=(Rectangle){x+(width-184)*scale,y+222*scale,80*scale,28*scale};
    l.apply=(Rectangle){x+(width-96)*scale,y+222*scale,84*scale,28*scale};
    l.valid=true; return l;
}

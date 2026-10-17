#include "ui/brand_geometry.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { fprintf(stderr,"FAIL brand:%d: %s\n",__LINE__,#test); exit(1); } } while (0)
static double cross(BrandPoint a,BrandPoint b,BrandPoint c) { return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
static bool inside(const BrandPoint *points,size_t count,BrandPoint p) {
    bool result=false;
    for (size_t i=0,j=count-1;i<count;j=i++) if ((points[i].y>p.y)!=(points[j].y>p.y) && p.x<(points[j].x-points[i].x)*(p.y-points[i].y)/(points[j].y-points[i].y)+points[i].x) result=!result;
    return result;
}
int main(void) {
    size_t shapes=sizeof(brand_shapes)/sizeof(*brand_shapes),vertices=sizeof(brand_vertices)/sizeof(*brand_vertices),indices=sizeof(brand_indices)/sizeof(*brand_indices);
    CHECK(shapes==20 && BRAND_MAX_POINTS<=256);
    for (size_t n=0;n<shapes;n++) {
        const BrandShape *s=&brand_shapes[n]; CHECK(s->count>=3 && s->count<=BRAND_MAX_POINTS && s->vertex+s->count<=vertices && s->index+s->indices<=indices && s->indices==3*(s->count-2));
        const BrandPoint *p=&brand_vertices[s->vertex]; double polygon=0,triangles=0;
        for (size_t i=0;i<s->count;i++) { BrandPoint a=p[i],b=p[(i+1)%s->count]; polygon+=(double)a.x*b.y-(double)b.x*a.y; }
        bool valid=true;
        for (size_t i=0;i<s->indices;i+=3) {
            const unsigned short *t=&brand_indices[s->index+i];
            if (t[0]>=s->count || t[1]>=s->count || t[2]>=s->count) { valid=false; break; }
            double area=cross(p[t[0]],p[t[1]],p[t[2]]); triangles+=area;
            BrandPoint middle={(p[t[0]].x+p[t[1]].x+p[t[2]].x)/3,(p[t[0]].y+p[t[1]].y+p[t[2]].y)/3};
            if (area<=0 || !inside(p,s->count,middle)) valid=false;
        }
        CHECK(valid && fabs(triangles-fabs(polygon))<.1);
        float sizes[]={16,20,24,32,48,64,128,256,512,1024},densities[]={1,1.25f,1.5f,2};
        for (size_t i=0;i<sizeof(sizes)/sizeof(*sizes);i++) for (size_t d=0;d<sizeof(densities)/sizeof(*densities);d++) {
            float pixels=sizes[i]*densities[d]; BrandTransform transform=brand_transform(s,pixels); bool finite=true;
            for (size_t v=0;v<s->count;v++) {
                float x=p[v].x*transform.sx+transform.tx,y=p[v].y*transform.sy+transform.ty;
                if (!isfinite(x) || !isfinite(y) || x<0 || x>256 || y<0 || y>256) finite=false;
            }
            CHECK(finite && transform.sx>0 && transform.sy>0);
            if (s->mode==1) CHECK((s->x1-s->x0)*transform.sx*pixels/256>=.7999f);
            if (s->mode==2) CHECK((s->y1-s->y0)*transform.sy*pixels/256>=.7999f);
            if (s->mode==4) CHECK((s->y0*transform.sy+transform.ty-149)*pixels/256>=.6999f);
            if (pixels>=64) CHECK(transform.sx==1 && transform.sy==1 && transform.tx==0 && transform.ty==0);
        }
    }
    printf("PASS: %u icon geometry checks (triangulation, bounds, optical gaps and 1/1.25/1.5/2x scales)\n",checks);
    return 0;
}

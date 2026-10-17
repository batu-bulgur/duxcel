#include "brand.h"
#include "brand_geometry.h"
#include "brand_bitmap.h"
static Texture2D cache;
bool brand_init(void) {
    if (cache.id) return true;
    Image image=LoadImageFromMemory(".png",brand_bitmap,sizeof(brand_bitmap)); if (!image.data) return false;
    cache=LoadTextureFromImage(image); UnloadImage(image);
    if (cache.id) { GenTextureMipmaps(&cache); SetTextureFilter(cache,TEXTURE_FILTER_TRILINEAR); }
    return cache.id!=0;
}
void brand_shutdown(void) { if (cache.id) UnloadTexture(cache); cache=(Texture2D){0}; }
void brand_draw_cached(Rectangle bounds) {
    if (!cache.id) { brand_draw(bounds,1); return; }
    if (!isfinite(bounds.x) || !isfinite(bounds.y) || !isfinite(bounds.width) || !isfinite(bounds.height) || bounds.width<=0 || bounds.height<=0) return;
    float side=fminf(bounds.width,bounds.height);
    Rectangle target={bounds.x+(bounds.width-side)*.5f,bounds.y+(bounds.height-side)*.5f,side,side};
    DrawTexturePro(cache,(Rectangle){0,0,(float)cache.width,(float)cache.height},target,(Vector2){0},0,WHITE);
}

void brand_draw(Rectangle bounds,float pixel_ratio) {
    if (!isfinite(bounds.x) || !isfinite(bounds.y) || !isfinite(bounds.width) || !isfinite(bounds.height) || bounds.width<=0 || bounds.height<=0) return;
    float side=fminf(bounds.width,bounds.height),scale=side/256;
    float x=bounds.x+(bounds.width-side)*.5f,y=bounds.y+(bounds.height-side)*.5f;
    if (!isfinite(pixel_ratio) || pixel_ratio<=0) pixel_ratio=1;
    Vector2 points[BRAND_MAX_POINTS];
    for (size_t shape=0;shape<sizeof(brand_shapes)/sizeof(*brand_shapes);shape++) {
        const BrandShape *s=&brand_shapes[shape]; BrandTransform t=brand_transform(s,side*pixel_ratio);
        for (size_t i=0;i<s->count;i++) {
            BrandPoint p=brand_vertices[s->vertex+i]; points[i]=(Vector2){x+(p.x*t.sx+t.tx)*scale,y+(p.y*t.sy+t.ty)*scale};
        }
        Color color={s->color[0],s->color[1],s->color[2],s->color[3]};
        for (size_t i=0;i<s->indices;i+=3) {
            const unsigned short *tri=&brand_indices[s->index+i];
            DrawTriangle(points[tri[2]],points[tri[1]],points[tri[0]],color);
        }
    }
}

#include "../ui/brand.h"
#include "platform.h"
#include "brand_images.h"

bool brand_install_icon(void) {
#ifdef __APPLE__
    return dx_native_set_icon(brand_icns,sizeof(brand_icns));
#else
    const unsigned char *data[]={brand_png_16,brand_png_32,brand_png_64,brand_png_128,brand_png_256,brand_png_512};
    const int lengths[]={sizeof(brand_png_16),sizeof(brand_png_32),sizeof(brand_png_64),sizeof(brand_png_128),sizeof(brand_png_256),sizeof(brand_png_512)};
    Image images[6]={0}; int count=0;
    for (int i=0;i<6;i++) {
        Image image=LoadImageFromMemory(".png",data[i],lengths[i]);
        if (image.data) images[count++]=image;
    }
    if (count) SetWindowIcons(images,count);
    for (int i=0;i<count;i++) UnloadImage(images[i]);
    return count==6;
#endif
}

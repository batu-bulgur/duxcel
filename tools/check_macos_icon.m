#import <AppKit/AppKit.h>
#include <stdio.h>
int main(int argc,char **argv) {
    @autoreleasepool {
        if (argc!=2) { fprintf(stderr,"Pass the built Duxcel.app path.\n"); return 1; }
        NSString *path=[NSString stringWithUTF8String:argv[1]];
        if (![path isAbsolutePath]) path=[NSFileManager.defaultManager.currentDirectoryPath stringByAppendingPathComponent:path];
        NSBundle *bundle=[NSBundle bundleWithPath:path]; NSString *name=[bundle objectForInfoDictionaryKey:@"CFBundleIconFile"];
        if (![name isEqualToString:@"duxcel.icns"]) { fprintf(stderr,"Missing bundle icon registration.\n"); return 1; }
        NSImage *image=[[NSImage alloc] initWithContentsOfFile:[bundle.resourcePath stringByAppendingPathComponent:name]];
        if (!image || !image.isValid) { fprintf(stderr,"AppKit rejected the bundle ICNS.\n"); [image release]; return 1; }
        bool small=false,retina=false;
        for (NSImageRep *representation in image.representations) {
            NSInteger width=representation.pixelsWide,height=representation.pixelsHigh;
            printf("AppKit icon representation: %ldx%ld pixels\n",(long)width,(long)height);
            if (width!=height) { [image release]; return 1; }
            small|=width==16; retina|=width==1024;
        }
        [image release];
        if (!small || !retina) { fprintf(stderr,"AppKit did not expose the small and Retina icon representations.\n"); return 1; }
        puts("PASS: native AppKit decodes the registered bundle icon, including 16px and 1024px representations"); return 0;
    }
}

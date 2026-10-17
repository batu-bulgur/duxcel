/* Native build-time renderer for the original Carbon SVG assets. The shipped
   application loads the checked-in atlas; it does not parse SVG or use AppKit
   to draw its widgets. Regeneration needs only the existing macOS compiler. */
#import <AppKit/AppKit.h>
int main(int argc,const char **argv) {
    @autoreleasepool {
        if (argc!=3) { fprintf(stderr,"usage: render-ui-icons assets/ui output.png\n"); return 2; }
        NSString *root=[NSString stringWithUTF8String:argv[1]];
        NSData *data=[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"carbon-icons.json"]];
        NSDictionary *manifest=data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
        NSArray *names=manifest[@"names"]; NSInteger cell=[manifest[@"cell"] integerValue],columns=[manifest[@"columns"] integerValue];
        if (!names.count || cell!=64 || columns!=8) return 1;
        NSInteger width=cell*columns,height=cell*((names.count+columns-1)/columns);
        NSBitmapImageRep *bitmap=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL pixelsWide:width pixelsHigh:height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
        NSGraphicsContext *context=[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
        [NSGraphicsContext saveGraphicsState]; [NSGraphicsContext setCurrentContext:context];
        context.imageInterpolation=NSImageInterpolationHigh;
        for (NSUInteger i=0;i<names.count;i++) {
            @autoreleasepool {
                NSString *path=[root stringByAppendingPathComponent:[NSString stringWithFormat:@"carbon/%@.svg",names[i]]];
                NSImage *icon=[[NSImage alloc] initWithContentsOfFile:path];
                if (!icon) { fprintf(stderr,"Cannot decode %s\n",path.UTF8String); return 1; }
                NSRect rect=NSMakeRect((i%columns)*cell,height-(i/columns+1)*cell,cell,cell);
                [icon drawInRect:rect fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1];
            }
        }
        [NSGraphicsContext restoreGraphicsState];
        NSData *png=[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        return [png writeToFile:[NSString stringWithUTF8String:argv[2]] atomically:YES] ? 0 : 1;
    }
}

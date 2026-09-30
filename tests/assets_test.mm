#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#include <cassert>
#include <cstdio>
int main(int argc,char **argv) {
    @autoreleasepool {
        assert(argc==2 || argc==3);
        bool single=argc==3;
        NSURL *url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]];
        CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)url,NULL); assert(source);
        NSDictionary *opts=@{(id)kCGImageSourceCreateThumbnailFromImageAlways:@YES,(id)kCGImageSourceThumbnailMaxPixelSize:@512};
        CGImageRef atlas=CGImageSourceCreateThumbnailAtIndex(source,0,(__bridge CFDictionaryRef)opts); assert(atlas);
        assert(CGImageGetWidth(atlas)>0 && CGImageGetHeight(atlas)>0);
        if(!single) assert(CGImageGetWidth(atlas)==512 && CGImageGetHeight(atlas)==512);
        for(int i=0;i<(single?1:4);++i) {
            CGImageRef frame=CGImageCreateWithImageInRect(atlas,single?CGRectMake(0,0,CGImageGetWidth(atlas),CGImageGetHeight(atlas)):CGRectMake((i%2)*256,(i/2)*256,256,256)); assert(frame);
            unsigned char pixels[256*256*4]{};
            CGColorSpaceRef colors=CGColorSpaceCreateDeviceRGB();
            CGContextRef ctx=CGBitmapContextCreate(pixels,256,256,8,256*4,colors,kCGImageAlphaPremultipliedLast); assert(ctx);
            CGContextDrawImage(ctx,CGRectMake(0,0,256,256),frame);
            int transparent=0,visible=0;
            for(int p=0;p<256*256;++p) { if(pixels[p*4+3]<10) ++transparent; if(pixels[p*4+3]>200) ++visible; }
            assert(transparent>1000 && visible>1000);
            printf("frame %d: %d transparent, %d visible pixels\n",i,transparent,visible);
            CGContextRelease(ctx); CGColorSpaceRelease(colors); CGImageRelease(frame);
        }
        CGImageRelease(atlas); CFRelease(source);
    }
}

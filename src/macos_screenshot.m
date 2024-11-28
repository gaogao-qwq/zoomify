#import "macos_screenshot.h"
#include <CoreGraphics/CGImage.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <objc/runtime.h>

@interface INScreenshotContext : NSObject

@property(copy) NSMutableData *data;
@property NSInteger posx;
@property NSInteger posy;
@property size_t width;
@property size_t height;
@property bool isPrimary;

@end

@implementation INScreenshotContext

- (instancetype)init {
    self = [super init];
    if (self) {
        self.data = [[NSMutableData alloc] init];
        self.posx = 0;
        self.posy = 0;
        self.width = 0;
        self.height = 0;
        self.isPrimary = false;
    }
    return self;
}

- (instancetype)initWithData:(NSMutableData *)data posX:(NSInteger)x posY:(NSInteger)y width:(size_t)w height:(size_t)h isPrimary:(bool)primary {
    self = [super init];
    if (self) {
        _data = data;
        _posx = x;
        _posy = y;
        _width = w;
        _height = h;
        _isPrimary = primary;
    }
    return self;
}

@end

/* utility extension for debugging */
@implementation NSObject (propertiesToString)
- (NSString *)propertiesToString {
    NSMutableString *ret = [NSMutableString stringWithString:@""];
    @autoreleasepool {
        uint32 propertyCount;
        objc_property_t *propertyArray = class_copyPropertyList([self class], &propertyCount);
        for (uint32 i = 0; i < propertyCount; ++i) {
            objc_property_t property = propertyArray[i];
            NSString *propertyName = [[NSString alloc] initWithUTF8String:property_getName(property)];
            @try {
                [ret appendFormat:@"property: %@, value: %@\n", propertyName, [self valueForKey:propertyName]];
            } @catch (NSException *exception) {
                continue;
            }
        }
        free(propertyArray);
    }
    return ret;
}
@end

ScreenshotContext *captureScreenshot(size_t *count) {
    __block NSMutableArray<INScreenshotContext *> *ctxArray = [NSMutableArray array];
    __block bool done = false;
    NSArray<NSScreen *> *screens = [NSScreen screens];

    [SCShareableContent getShareableContentWithCompletionHandler:^(
                            SCShareableContent *_Nullable shareableContent,
                            NSError *_Nullable error) {
      if (error) {
          NSLog(@"error: %@", [error localizedDescription]);
          done = true;
          return;
      }

      NSUInteger displayCount = [shareableContent.displays count];
      __block NSUInteger doneCount = 0;

      // take screenshot for all screen
      for (NSScreen *screen in screens) {
          SCDisplay *display = NULL;
          for (SCDisplay *dis in shareableContent.displays) {
              if (dis.displayID != [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue]) continue;
              display = dis;
              break;
          }
          if (display == NULL) continue;

          SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
          SCStreamConfiguration *config;

          config = [[SCStreamConfiguration alloc] init];
          config.width = filter.contentRect.size.width * filter.pointPixelScale;
          config.height = filter.contentRect.size.height * filter.pointPixelScale;
          config.pixelFormat = kCVPixelFormatType_64RGBAHalf;
          config.colorSpaceName = kCGColorSpaceExtendedDisplayP3;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_15_0
          config.captureDynamicRange = SCCaptureDynamicRangeHDRLocalDisplay;
#endif

#if defined(DEBUG)
            // NSLog(@"%@", [config propertiesToString]);
#endif

          [SCScreenshotManager
              captureImageWithFilter:filter
                       configuration:config
                   completionHandler:^(CGImageRef _Nullable sampleBuffer, NSError *_Nullable error) {
                     if (error) {
                         NSLog(@"Failed to take screenshot: %@.", [error localizedDescription]);
                         if (++doneCount == displayCount) done = true;
                         return;
                     }

                     // create PNG destination context
                     NSMutableData *pngData = [NSMutableData data];
                     CGImageDestinationRef dest = CGImageDestinationCreateWithData(
                         (__bridge CFMutableDataRef)pngData, (CFStringRef)UTTypePNG.identifier, 1, nil);
                     if (!dest) {
                         NSLog(@"Failed to take screenshot: PNG destination creation failed.");
                         if (++doneCount == displayCount) done = true;
                         return;
                     }

                     // add CGImage to destination
                     CGImageDestinationAddImage(dest, sampleBuffer, nil);
                     if (CGImageDestinationFinalize(dest)) {
                         INScreenshotContext *ctx = [[INScreenshotContext alloc]
                             initWithData:pngData
                                     posX:screen.frame.origin.x
                                     posY:screen.frame.origin.y
                                    width:CGImageGetWidth(sampleBuffer)
                                   height:CGImageGetHeight(sampleBuffer)
                                isPrimary:[NSScreen mainScreen] == screen];
                         [ctxArray addObject:ctx];
                         NSLog(@"%ld, %ld", ctx.width, ctx.height);
                     } else {
                         NSLog(@"Failed to take screenshot: finalize image failed.");
                     }
                     if (++doneCount == displayCount) done = true;
                   }];
      }
    }];

    while (!done) {
        [NSThread sleepForTimeInterval:0.01];
    }

    *count = [ctxArray count];
    if (!*count) return NULL;

    ScreenshotContext *contextArray = malloc(*count * sizeof(ScreenshotContext));
    for (size_t i = 0; i < *count; ++i) {
        size_t length = [ctxArray[i].data length];
        // copy screenshot image data to c array
        contextArray[i].data = malloc(length);
        memcpy(contextArray[i].data, [ctxArray[i].data bytes], length);
        contextArray[i].posx = (int)ctxArray[i].posx;
        contextArray[i].posy = (int)ctxArray[i].posy;
        contextArray[i].width = ctxArray[i].width;
        contextArray[i].height = ctxArray[i].height;
        contextArray[i].isPrimary = ctxArray[i].isPrimary;
        contextArray[i].size = length;
    }

    return contextArray;
}

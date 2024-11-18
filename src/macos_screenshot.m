#import "macos_screenshot.h"
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <objc/runtime.h>

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

const ScreenshotContext *captureScreenshot(size_t *count) {
    __block NSMutableArray<NSMutableData *> *screenshot = [NSMutableArray array];
    __block bool done = false;

    [SCShareableContent getShareableContentWithCompletionHandler:^(
                            SCShareableContent *_Nullable shareableContent,
                            NSError *_Nullable error) {
      if (error) {
          NSLog(@"error: %@", [error localizedDescription]);
          done = true;
          return;
      }

      // take screenshot for all display
      NSUInteger displayCount = [shareableContent.displays count];
      __block NSUInteger doneCount = 0;
      for (SCDisplay *display in shareableContent.displays) {
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
          NSLog(@"%@", [config propertiesToString]);
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
                     CGImageDestinationRef dest = CGImageDestinationCreateWithData((__bridge CFMutableDataRef)pngData, (CFStringRef)UTTypePNG.identifier, 1, nil);
                     if (!dest) {
                         NSLog(@"Failed to take screenshot: PNG destination creation failed.");
                         if (++doneCount == displayCount) done = true;
                         return;
                     }

                     // add CGImage to destination
                     CGImageDestinationAddImage(dest, sampleBuffer, nil);
                     if (CGImageDestinationFinalize(dest)) {
                         [screenshot addObject:pngData];
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

    *count = [screenshot count];
    if (!*count) return NULL;

    ScreenshotContext *contextArray = malloc(*count * sizeof(ScreenshotContext));
    for (size_t i = 0; i < *count; ++i) {
        NSLog(@"%lu", [screenshot[i] length]);
        size_t length = [screenshot[i] length];
        // copy screenshot image data to c array
        contextArray[i].data = malloc(length);
        memcpy(contextArray[i].data, [screenshot[i] bytes], length);
        contextArray[i].size = length;
    }

    return contextArray;
}

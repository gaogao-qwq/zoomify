#import "macos_screenshot.h"
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

const ScreenshotContext *captureScreenshot(size_t *count) {
    __block NSMutableArray *screenshot = [NSMutableArray array];
    __block bool done = false;

    [SCShareableContent getShareableContentWithCompletionHandler:^(
                            SCShareableContent *_Nullable shareableContent,
                            NSError *_Nullable error) {
      if (error) {
          NSLog(@"error: %@", [error localizedDescription]);
          done = true;
          return;
      }

      SCDisplay *display = shareableContent.displays[0];
      SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
      SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
      config.width = display.width;
      config.height = display.height;
      config.captureResolution = SCCaptureResolutionBest;

      [SCScreenshotManager
          captureImageWithFilter:filter
                   configuration:config
               completionHandler:^(CGImageRef _Nullable sampleBuffer, NSError *_Nullable error) {
                 if (error) {
                     NSLog(@"error: %@", [error localizedDescription]);
                     done = true;
                     return;
                 }

                 // create PNG destination context
                 NSMutableData *pngData = [NSMutableData data];
                 CGImageDestinationRef dest = CGImageDestinationCreateWithData((__bridge CFMutableDataRef)pngData, (CFStringRef)UTTypePNG.identifier, 1, nil);
                 if (!dest) {
                     NSLog(@"dest is nil");
                     done = true;
                     return;
                 }

                 // add CGImage to destination
                 CGImageDestinationAddImage(dest, sampleBuffer, nil);
                 if (CGImageDestinationFinalize(dest)) {
                     [screenshot addObject:pngData];
                 } else {
                     NSLog(@"failed to finalize image to destination");
                 }

                 done = true;
               }];
    }];

    while (!done) {
        [NSThread sleepForTimeInterval:0.01];
    }

    *count = [screenshot count];
    if (!*count) return NULL;

    ScreenshotContext *contextArray = malloc(*count * sizeof(ScreenshotContext));
    for (size_t i = 0; i < *count; ++i) {
        size_t length = [screenshot[i] length];
        // copy screenshot image data to c array
        contextArray[i].data = malloc(length);
        memcpy(contextArray[i].data, [screenshot[i] bytes], length);
        contextArray->size = length;
    }

    return contextArray;
}

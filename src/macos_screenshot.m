#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

const char **captureScreenshot(size_t *length) {
    __block NSMutableArray *urlArray = [NSMutableArray array];
    __block bool done = false;

    [SCShareableContent getShareableContentWithCompletionHandler:^(
                            SCShareableContent *_Nullable shareableContent,
                            NSError *_Nullable error) {
      if (error) {
          NSLog(@"error: %@", [error localizedDescription]);
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

                 // get formatted date string
                 NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
                 [dateFormatter setDateFormat:@"yyyy-MM-dd_HH:mm:ss"];
                 NSString *dateString = [dateFormatter stringFromDate:[NSDate date]];

                 // get cache directory path
                 NSArray<NSString *> *cacheDirs = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
                 NSString *cacheDirPath = [cacheDirs firstObject];

                 NSURL *outputURL = [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@/%@.png", cacheDirPath, dateString]];
                 CGImageDestinationRef dest = CGImageDestinationCreateWithURL((CFURLRef)outputURL, (CFStringRef)UTTypePNG.identifier, 1, nil);
                 if (!dest) {
                     NSLog(@"dest is nil");
                     done = true;
                     return;
                 }

                 CGImageDestinationAddImage(dest, sampleBuffer, nil);
                 if (CGImageDestinationFinalize(dest)) {
                     [urlArray addObject:outputURL.path];
                 }
                 done = true;
               }];
    }];

    while (!done) {
        [NSThread sleepForTimeInterval:0.1];
    }

    *length = [urlArray count];
    const char **ret = malloc(*length * sizeof(char *));
    for (size_t i = 0; i < *length; ++i) {
        ret[i] = strdup([urlArray[i] UTF8String]);
    }
    return ret;
}

void deallocStringArray(char **array, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        free(array[i]);
    }
    free(array);
}

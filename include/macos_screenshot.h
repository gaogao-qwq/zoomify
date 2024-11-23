#ifndef MACOS_SCREENSHOT_H
#define MACOS_SCREENSHOT_H_ 1

#include <stddef.h>

typedef struct ScreenshotContext {
    unsigned char *data; /* image data */
    size_t size;         /* image size */
    int posx;
    int posy;
} ScreenshotContext;

extern const ScreenshotContext *captureScreenshot(size_t *);

#endif

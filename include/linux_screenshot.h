#ifndef LINUX_SCREENSHOT_H
#define LINUX_SCREENSHOT_H_ 1

#include <stddef.h>

typedef struct ScreenshotContext {
    unsigned char *data; /* image data */
    size_t size;         /* image size */
    int posx;
    int posy;
    int width;
    int height
} ScreenshotContext;

ScreenshotContext *captureScreenshot(size_t *);

#endif

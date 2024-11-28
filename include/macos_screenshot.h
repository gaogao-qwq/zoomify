#ifndef MACOS_SCREENSHOT_H
#define MACOS_SCREENSHOT_H_ 1

#include <stddef.h>
#include <stdbool.h>

typedef struct ScreenshotContext {
    unsigned char *data; /* image data */
    size_t size;         /* image size */
    int posx;
    int posy;
    size_t width;
    size_t height;
    bool isPrimary;
} ScreenshotContext;

extern ScreenshotContext *captureScreenshot(size_t *);

#endif

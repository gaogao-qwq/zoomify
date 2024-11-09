#ifndef MACOS_SCREENSHOT_H
#define MACOS_SCREENSHOT_H_ 1

#include <stddef.h>

const char **captureScreenshot(size_t *);
void deallocStringArray(char **, size_t);

#endif

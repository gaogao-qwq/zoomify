#include "linux_screenshot.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <stdio.h>
#include <stdlib.h>

// raylib is compiled with stb_image_write.h, thus there's no need to include
extern unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

const ScreenshotContext *captureScreenshot(size_t *count) {
    Display *display;
    Window root;
    XImage *image;
    int x, y, width, height;

    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Failed to open display\n");
        return NULL;
    }

    int scr_cnt;
    XineramaScreenInfo *scr_info = XineramaQueryScreens(display, &scr_cnt);
    if (scr_info == NULL) {
        fprintf(stderr, "Failed to query screens: Xinerama is not active\n");
        goto query_screen_failed;
    }
    ScreenshotContext *context_array = malloc(scr_cnt * sizeof(ScreenshotContext));
    *count = scr_cnt;

    for (int i = 0; i < scr_cnt; ++i) {
        x = scr_info[i].x_org;
        y = scr_info[i].y_org;
        width = scr_info[i].width;
        height = scr_info[i].height;
        fprintf(stderr, "screen %d: (%d, %d) %dx%d\n", i, x, y, width, height);

        root = DefaultRootWindow(display);
        image = XGetImage(display, root, x, y, width, height, AllPlanes, ZPixmap);
        if (image == NULL) {
            fprintf(stderr, "Failed to get image\n");
            goto get_image_failed;
        }

        size_t rgba_size = width * height * 4;
        unsigned char *rgba_data = (unsigned char *)malloc(rgba_size);
        if (rgba_data == NULL) {
            fprintf(stderr, "Failed to alloc memory for rgba_data\n");
            goto alloc_rgba_failed;
        }

        // convert XImage to RGBA
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                long pixel = XGetPixel(image, x, y);
                rgba_data[(y * width + x) * 4 + 0] = (pixel & image->red_mask) >> 16;   // R
                rgba_data[(y * width + x) * 4 + 1] = (pixel & image->green_mask) >> 8;  // G
                rgba_data[(y * width + x) * 4 + 2] = (pixel & image->blue_mask);        // B
                rgba_data[(y * width + x) * 4 + 3] = 255;                               // A
            }
        }

        // convert RGBA to PNG format byte array
        int png_size;
        unsigned char *png_data = stbi_write_png_to_mem(rgba_data, width * 4, width, height, 4, &png_size);
        if (png_data == NULL) {
            fprintf(stderr, "Failed to convert rgba to png\n");
            goto convert_png_failed;
        }
        context_array[i].data = png_data;
        context_array[i].size = (size_t)png_size;
        context_array[i].posx = x;
        context_array[i].posy = y;

        // free memory
        free(rgba_data);
        XDestroyImage(image);
    }

    XFree(scr_info);
    XCloseDisplay(display);
    return context_array;

convert_png_failed:
alloc_rgba_failed:
    XDestroyImage(image);
get_image_failed:
    XCloseDisplay(display);
query_screen_failed:
    XFree(scr_info);
    return NULL;
}

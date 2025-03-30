#include "linux_screenshot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// raylib is compiled with stb_image_write.h, thus there's no need to include
extern unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

#ifdef X11
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>

ScreenshotContext *captureScreenshotX11(size_t *count) {
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
        context_array[i].width = width;
        context_array[i].height = height;
        context_array[i].posx = x;
        context_array[i].posy = y;
        context_array[i].isPrimary = !scr_info[i].screen_number;

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
#endif  // X11

#ifdef WAYLAND
#include <dbus/dbus.h>
#include <wayland-client.h>

// Display info part of wayland implementation uses code from
// https://github.com/eklitzke/wlinfo, which is released under
// the MIT license below:
//
// Copyright © 2016 Evan Klitzke
// Copyright © 2008-2012 Kristian Høgsberg
// Copyright © 2010-2012 Intel Corporation
// Copyright © 2010-2011 Benjamin Franzke
// Copyright © 2011-2012 Collabora, Ltd.
// Copyright © 2010 Red Hat <mjg@redhat.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// ---
//
// The above is the version of the MIT "Expat" License used by X.org:
//
//     http://cgit.freedesktop.org/xorg/xserver/tree/COPYING

typedef struct WaylandDisplayInfo {
    int id;
    int x;
    int y;
    int width;
    int height;
} WaylandDisplayInfo;

struct wl_display_context {
    struct wl_list outputs;
};

struct output_t {
    int id;
    int x;
    int y;
    int width;
    int height;
    struct wl_display_context *ctx;
    struct wl_output *output;
    struct wl_list link;
};

static void output_handle_geometry(void *data, [[maybe_unused]] struct wl_output *wl_output,
                                   int32_t x, int32_t y, [[maybe_unused]] int32_t physical_width,
                                   [[maybe_unused]] int32_t physical_height, [[maybe_unused]] int32_t subpixel,
                                   [[maybe_unused]] const char *make, [[maybe_unused]] const char *model,
                                   [[maybe_unused]] int32_t output_transform) {
    struct output_t *out = (struct output_t *)data;
    out->x = x;
    out->y = y;
}

static void output_handle_mode(void *data, [[maybe_unused]] struct wl_output *wl_output,
                               [[maybe_unused]] uint32_t flags, int32_t width, int32_t height,
                               [[maybe_unused]] int32_t refresh) {
    struct output_t *out = (struct output_t *)data;
    out->width = width;
    out->height = height;
}

static void output_handle_done([[maybe_unused]] void *data, [[maybe_unused]] struct wl_output *wl_output) {}

static void output_handle_scale([[maybe_unused]] void *data, [[maybe_unused]] struct wl_output *wl_output, [[maybe_unused]] int32_t scale) {}

static void output_handle_description([[maybe_unused]] void *data, [[maybe_unused]] struct wl_output *wl_output, [[maybe_unused]] const char *description) {}

static void output_handle_name([[maybe_unused]] void *data, [[maybe_unused]] struct wl_output *wl_output, [[maybe_unused]] const char *name) {}

static const struct wl_output_listener output_listener = {
    .geometry = output_handle_geometry,
    .mode = output_handle_mode,
    .done = output_handle_done,
    .scale = output_handle_scale,
    .description = output_handle_description,
    .name = output_handle_name,
};

static void global_registry_handler(void *data, struct wl_registry *registry,
                                    uint32_t id, const char *interface,
                                    uint32_t version) {
    if (!strcmp(interface, "wl_output")) {
        struct wl_display_context *ctx = (struct wl_display_context *)data;
        struct output_t *output = malloc(sizeof(struct output_t));
        output->ctx = ctx;
        output->id = id;
        output->output = wl_registry_bind(registry, id, &wl_output_interface, version);
        wl_list_insert(&ctx->outputs, &output->link);
        wl_output_add_listener(output->output, &output_listener, output);
    }
}

static void global_registry_remover([[maybe_unused]] void *data, [[maybe_unused]] struct wl_registry *registry, [[maybe_unused]] uint32_t id) {}

static const struct wl_registry_listener registry_listener = {
    .global = global_registry_handler,
    .global_remove = global_registry_remover,
};

WaylandDisplayInfo *getDisplayInfo() {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_display_context ctx;
    struct output_t *out, *tmp;
    WaylandDisplayInfo *infos = NULL;

    // get wayland display info
    if ((display = wl_display_connect(NULL)) == NULL) {
        fprintf(stderr, "Failed to connect to wayland display\n");
        goto connect_wayland_display_failed;
    }

    wl_list_init(&ctx.outputs);
    if ((registry = wl_display_get_registry(display)) == NULL) {
        fprintf(stderr, "Failed to get wayland registry\n");
        goto get_wayland_registry_failed;
    }
    wl_registry_add_listener(registry, &registry_listener, &ctx);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    int len = wl_list_length(&ctx.outputs);
    printf("screen count: %d\n", len);
    int i = 0;
    infos = (WaylandDisplayInfo *)malloc(sizeof(WaylandDisplayInfo) * len);
    wl_list_for_each_safe(out, tmp, &ctx.outputs, link) {
        infos[i].id = out->id;
        infos[i].width = out->width;
        infos[i].height = out->height;
        infos[i].x = out->x;
        infos[i].y = out->y;
        printf("id: %d, width: %d, height: %d, x: %d, y: %d\n", out->id, out->width, out->height, out->x, out->y);
        wl_output_destroy(out->output);
        wl_list_remove(&out->link);
        free(out);
        ++i;
    }

    wl_registry_destroy(registry);
get_wayland_registry_failed:
    wl_display_disconnect(display);
connect_wayland_display_failed:
    return infos;
}

/*
 * @brief Take screenshot via xdg-desktop-portal through dbus
 * org.freedesktop.impl.portal.Screenshot.Screenshot
 * https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Screenshot.html
 *
 * Screenshot (
 *   IN parent_window s,
 *   IN options a{sv},
 *   OUT handle o
 * )
 *
 * Response (
 *   response u,
 *   results a{sv}
 * )
 **/
ScreenshotContext *captureScreenshotWayland(size_t *count) {
    DBusError error;
    DBusConnection *conn;
    DBusMessageIter args_iter, options_iter, entries_iter, variants_iter, reply_iter, response_iter, response_dict_iter, screenshot_uri_value_iter;
    const char *key, *screenshot_uri_value = "", *handle;
    ScreenshotContext *screenshot_context = NULL;
    FILE *fp;
    long nbytes;
    *count = 0;

    WaylandDisplayInfo *display_infos = getDisplayInfo();
    if (display_infos == NULL) {
        fprintf(stderr, "Failed to get wayland display info");
        goto get_display_info_failed;
    }

    // capture screenshot via xdg-desktop-portal
    dbus_error_init(&error);
    conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Failed to connect to dbus :%s", error.message);
        goto connect_dbus_failed;
    }
    if (conn == NULL) {
        fprintf(stderr, "Failed to connect to dbus: conn == NULL");
        goto connect_dbus_failed;
    }

    DBusMessage *screenshot_message;
    screenshot_message = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot",
        "Screenshot");
    if (screenshot_message == NULL) {
        fprintf(stderr, "Failed to create dbus message\n");
        goto create_message_failed;
    }

    const char *parent_window = "";
    dbus_message_append_args(screenshot_message,
                             DBUS_TYPE_STRING, &parent_window,
                             DBUS_TYPE_INVALID);
    // clang-format off
    dbus_message_iter_init_append(screenshot_message, &args_iter);
    dbus_message_iter_open_container(&args_iter, DBUS_TYPE_ARRAY, "{sv}", &options_iter);
        // modal (b)
        dbus_message_iter_open_container(&options_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entries_iter);
            const char *modal_key = "modal";
            int modal_value = 1;
            dbus_message_iter_append_basic(&entries_iter, DBUS_TYPE_STRING, &modal_key);
            dbus_message_iter_open_container(&entries_iter, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &variants_iter);
                dbus_message_iter_append_basic(&variants_iter, DBUS_TYPE_BOOLEAN, &modal_value);
            dbus_message_iter_close_container(&entries_iter, &variants_iter);
        dbus_message_iter_close_container(&options_iter, &entries_iter);

        // interactive (b)
        dbus_message_iter_open_container(&options_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entries_iter);
            const char *interactive_key = "interactive";
            int interactive_value = 0;
            dbus_message_iter_append_basic(&entries_iter, DBUS_TYPE_STRING, &interactive_key);
            dbus_message_iter_open_container(&entries_iter, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &variants_iter);
                dbus_message_iter_append_basic(&variants_iter, DBUS_TYPE_BOOLEAN, &interactive_value);
            dbus_message_iter_close_container(&entries_iter, &variants_iter);
        dbus_message_iter_close_container(&options_iter, &entries_iter);

        // permission_store_checked (b)
        dbus_message_iter_open_container(&options_iter, DBUS_TYPE_DICT_ENTRY, NULL, &entries_iter);
            const char *permission_store_checked_key = "permission_store_checked";
            int permission_store_checked_value = 1;
            dbus_message_iter_append_basic(&entries_iter, DBUS_TYPE_STRING, &permission_store_checked_key);
            dbus_message_iter_open_container(&entries_iter, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &variants_iter);
                dbus_message_iter_append_basic(&variants_iter, DBUS_TYPE_BOOLEAN, &permission_store_checked_value);
            dbus_message_iter_close_container(&entries_iter, &variants_iter);
        dbus_message_iter_close_container(&options_iter, &entries_iter);
    dbus_message_iter_close_container(&args_iter, &options_iter);
    // clang-format on

    dbus_bus_add_match(conn, "type='signal',interface='org.freedesktop.Portal.Request'", &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Failed to add match: %s\n", error.message);
        goto add_match_failed;
    }

    DBusMessage *screenshot_reply = dbus_connection_send_with_reply_and_block(conn, screenshot_message, -1, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Failed to send request to dbus : %s", error.message);
        goto send_dbus_request_failed;
    }

    dbus_message_iter_init(screenshot_reply, &reply_iter);
    if (dbus_message_iter_get_arg_type(&reply_iter) != DBUS_TYPE_OBJECT_PATH) {
        fprintf(stderr, "Expected reply type is object path");
        goto unexpected_reply_type;
    }
    dbus_message_iter_get_basic(&reply_iter, &handle);

    while (true) {
        dbus_connection_read_write(conn, 0);
        DBusMessage *msg = dbus_connection_pop_message(conn);
        if (!msg) continue;
        if (!dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response")) continue;

        dbus_message_iter_init(msg, &response_iter);
        if (dbus_message_iter_get_arg_type(&response_iter) != DBUS_TYPE_UINT32) {
            fprintf(stderr, "Expected results type for msg.response is uint32\n");
            goto unexpected_response_type;
        }
        dbus_message_iter_next(&response_iter);

        if (dbus_message_iter_get_arg_type(&response_iter) != DBUS_TYPE_ARRAY) {
            fprintf(stderr, "Expected results type for msg.results is array\n");
            goto unexpected_response_type;
        }
        dbus_message_iter_recurse(&response_iter, &response_dict_iter);

        if (dbus_message_iter_get_arg_type(&response_dict_iter) != DBUS_TYPE_DICT_ENTRY) {
            fprintf(stderr, "Expected results type for msg.results[0] is dict entry\n");
            goto unexpected_response_type;
        }

        dbus_message_iter_recurse(&response_dict_iter, &entries_iter);
        dbus_message_iter_get_basic(&entries_iter, &key);
        dbus_message_iter_next(&entries_iter);

        if (!strcmp(key, "uri")) {
            dbus_message_iter_recurse(&entries_iter, &screenshot_uri_value_iter);
            dbus_message_iter_get_basic(&screenshot_uri_value_iter, &screenshot_uri_value);
            printf("Result key: %s, value: %s\n", key, screenshot_uri_value);
        }
        dbus_message_iter_next(&response_dict_iter);
        dbus_message_unref(msg);
        break;
    }

    // remove `file://' uri scheme prefix
    screenshot_uri_value = screenshot_uri_value + 7;
    fp = fopen(screenshot_uri_value, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", screenshot_uri_value);
        goto open_file_failed;
    }

    /* get screenshot file size */
    fseek(fp, 0L, SEEK_END);
    nbytes = ftell(fp);
    printf("screenshot size: %ld\n", nbytes);
    rewind(fp);

    /* read screenshot into memory & delete file */
    unsigned char *buf = malloc(sizeof(unsigned char) * nbytes);
    fread(buf, sizeof(unsigned char), nbytes, fp);
    fclose(fp);
    remove(screenshot_uri_value);

    screenshot_context = malloc(sizeof(ScreenshotContext));
    screenshot_context->posx = display_infos[0].x;
    screenshot_context->posy = display_infos[0].y;
    screenshot_context->width = display_infos[0].width;
    screenshot_context->height = display_infos[0].height;
    screenshot_context->data = buf;
    screenshot_context->size = nbytes;
    screenshot_context->isPrimary = true;
    *count = 1;

open_file_failed:
unexpected_response_type:
unexpected_reply_type:
    dbus_message_unref(screenshot_reply);
add_match_failed:
send_dbus_request_failed:
    dbus_message_unref(screenshot_message);
create_message_failed:
connect_dbus_failed:
    dbus_error_free(&error);
get_display_info_failed:
    return screenshot_context;
}
#endif  // Wayland

ScreenshotContext *captureScreenshot(size_t *count) {
    char *XDG_SESSION_TYPE = getenv("XDG_SESSION_TYPE");
    if (!strcmp(XDG_SESSION_TYPE, "wayland")) {
#ifdef WAYLAND
        return captureScreenshotWayland(count);
#endif
        fprintf(stderr,
                "You are using a binary that not build with Wayland support,\n"
                "try rebuild it without specifying DISPLAY_PROTOCOL environment variable\n");
        return NULL;
    }
    if (!strcmp(XDG_SESSION_TYPE, "x11")) {
#ifdef X11
        return captureScreenshotX11(count);
#endif
        fprintf(stderr,
                "You are using a binary that not build with X11 support,\n"
                "try rebuild it without specifying DISPLAY_PROTOCOL environment variable\n");
        return NULL;
    }
    return NULL;
}

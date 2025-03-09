#include "linux_screenshot.h"

#include <stdio.h>
#include <stdlib.h>

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

/*
 * @brief Take screenshot via xdg-desktop-portal by dbus
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
    DBusMessageIter args_iter, options_iter, entries_iter, variants_iter, reply_iter, response_iter, response_dict_iter, value_iter;
    const char *key, *handle;

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
    printf("handle: %s\n", handle);

    while (true) {
        dbus_connection_read_write(conn, 0);
        DBusMessage *msg = dbus_connection_pop_message(conn);
        if (!msg) continue;
        if (!dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response")) continue;

        printf("Recvived %s.%s signal\n", dbus_message_get_interface(msg), dbus_message_get_member(msg));
        dbus_message_iter_init(msg, &response_iter);
        printf("%d\n", dbus_message_iter_get_arg_type(&response_iter));
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

        dbus_message_iter_recurse(&entries_iter, &value_iter);
        if (dbus_message_iter_get_arg_type(&value_iter) == DBUS_TYPE_STRING) {
            const char *value;
            dbus_message_iter_get_basic(&value_iter, &value);
            printf("Result key: %s, value: %s\n", key, value);
        }
        dbus_message_iter_next(&response_dict_iter);
        dbus_message_unref(msg);
        break;
    }

unexpected_response_type:
unexpected_reply_type:
    dbus_message_unref(screenshot_reply);
add_match_failed:
send_dbus_request_failed:
    dbus_message_unref(screenshot_message);
create_message_failed:
connect_dbus_failed:
    dbus_error_free(&error);
    *count = 0;
    return NULL;
}
#endif  // Wayland

ScreenshotContext *captureScreenshot(size_t *count) {
#ifdef X11
    return captureScreenshotX11(count);
#endif
#ifdef WAYLAND
    return captureScreenshotWayland(count);
#endif
    return NULL;
}

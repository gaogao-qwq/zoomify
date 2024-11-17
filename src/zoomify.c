#include <limits.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <shaders.h>

#if defined(__APPLE__)
#include "macos_screenshot.h"
#elif defined(__linux__)
#include "linux_screenshot.h"
#endif

#define ZOOM_MIN ((float)0.01f)
#define ZOOM_MAX ((float)100.0f)
#define SPL_RADIUS_MIN ((float)1.0f)
#define SPL_RADIUS_MAX ((float)1000.0f)
#define SPL_OPACITY_MIN ((float)0.0f)
#define SPL_OPACITY_MAX ((float)0.9f)

static int screenWidth, screenHeight;
static int renderWidth, renderHeight;

static bool showSpotlight = false;
static bool showKeystrokeTips = true;
#if defined(DEBUG)
static bool showDebugInfo = true;
#elif defined(RELEASE)
static bool showDebugInfo = false;
#endif

struct InputContext {
    Vector2 mousePos;
    Vector2 mouseWorldPos;
    Vector2 mouseDelta;
    float wheelDelta;
} inputCtx = {0};

struct CameraContext {
    Camera2D camera;
    float targetZoom;
} cameraCtx = {0};

struct SpotlightShaderUniformLocationContext {
    int opacity;
    int center;
    int radius;
    int textureWidth;
    int textureHeight;
} splShaderLocCtx = {0};

struct SpotlightShaderContext {
    bool enable;
    float currentOpacity;
    float targetOpacity;
    float center[2];
    float currentRadius;
    float targetRadius;
    int textureWidth;
    int textureHeight;
} splShaderCtx = {.targetRadius = 100.0f};

static Shader splShader = {0};

static void getSpotlightShaderUniformLocation(void);
static void updateInputContext(void);
static void updateCameraContext(void);
static void updateSpotlightShaderContext(void);
static void setSpotlightShaderUniformValues(void);
static void handleInput(void);
static void drawDebugInfo(void);
static void drawKeystrokeTips(void);

int main(void) {
    size_t contextCnt;
    const ScreenshotContext *contextArray;

    contextArray = captureScreenshot(&contextCnt);
    if (!contextCnt) {
        fprintf(stderr, "failed to capture screenshot\n");
        return EXIT_FAILURE;
    }

    printf("screenshot count: %lu\n", contextCnt);
    for (size_t i = 0; i < contextCnt; ++i) {
        fprintf(stderr, "screenshot %lu size: %lu\n", i, contextArray[i].size);
    }

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "magnifier");
    ToggleFullscreen();

    /* load screenshot into memory */
    Image screenshot = LoadImageFromMemory(".png", contextArray[0].data, (int)contextArray[0].size);
    Texture2D screenshotTexture = LoadTextureFromImage(screenshot);

    /* free memory */
    for (size_t i = 0; i < contextCnt; ++i) {
        free(contextArray[i].data);
    }
    free((ScreenshotContext *)contextArray);
    UnloadImage(screenshot);

    /* load fragment shader */
    splShader = LoadShaderFromMemory(NULL, SPOTLIGHT_SHADER_SRC);
    /* get shader uniform location */
    getSpotlightShaderUniformLocation();

    /* get resolution info */
    int currentMonitor = GetCurrentMonitor();
    screenWidth = GetMonitorWidth(currentMonitor);
    screenHeight = GetMonitorHeight(currentMonitor);
    renderWidth = GetRenderWidth();
    renderHeight = GetRenderHeight();

    /* calculate camera zoom */
    cameraCtx.camera.zoom = fminf((float)renderWidth / (float)screenshotTexture.width, (float)renderHeight / (float)screenshotTexture.height);
    cameraCtx.targetZoom = cameraCtx.camera.zoom;

    RenderTexture2D splMask = LoadRenderTexture(screenWidth, screenHeight);

    // clang-format off
    while(!WindowShouldClose()) {
        updateInputContext();              /* update input context */
        handleInput();                     /* handle user input */
        updateCameraContext();             /* update camera context */
        updateSpotlightShaderContext();    /* update shader context */
        setSpotlightShaderUniformValues(); /* set shader uniform value */

        /* rendering */
        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode2D(cameraCtx.camera);
                DrawTexture(screenshotTexture, 0, 0, WHITE);
            EndMode2D();

            BeginShaderMode(splShader);
                DrawTextureRec(splMask.texture, (Rectangle){0, 0, (float)screenWidth, (float)screenHeight}, (Vector2){0, 0}, BLANK);
            EndShaderMode();

            DrawFPS(10, 10);

            if (showDebugInfo) {
                drawDebugInfo();
            }

            if (showKeystrokeTips) {
                drawKeystrokeTips();
            }
        EndDrawing();
    }
    // clang-format on

    /* unload everything */
    UnloadShader(splShader);
    UnloadRenderTexture(splMask);
    UnloadTexture(screenshotTexture);
    CloseWindow();
    return EXIT_SUCCESS;
}

void getSpotlightShaderUniformLocation(void) {
    splShaderLocCtx.opacity = GetShaderLocation(splShader, "opacity");
    splShaderLocCtx.center = GetShaderLocation(splShader, "center");
    splShaderLocCtx.radius = GetShaderLocation(splShader, "radius");
    splShaderLocCtx.textureWidth = GetShaderLocation(splShader, "textureWidth");
    splShaderLocCtx.textureHeight = GetShaderLocation(splShader, "textureHeight");
}

void updateInputContext(void) {
    inputCtx.mousePos = GetMousePosition();
    inputCtx.mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), cameraCtx.camera);
    inputCtx.mouseDelta = GetMouseDelta();
    inputCtx.wheelDelta = GetMouseWheelMove();
}

void updateCameraContext(void) {
    float deltaTime = GetFrameTime();

    if (cameraCtx.camera.zoom < cameraCtx.targetZoom) {
        cameraCtx.camera.zoom = Clamp(cameraCtx.camera.zoom + deltaTime * 5.0f, ZOOM_MIN, cameraCtx.targetZoom);
    } else {
        cameraCtx.camera.zoom = Clamp(cameraCtx.camera.zoom - deltaTime * 5.0f, cameraCtx.targetZoom, ZOOM_MAX);
    }
}

void updateSpotlightShaderContext(void) {
    float deltaTime = GetFrameTime();

    if (splShaderCtx.currentOpacity < splShaderCtx.targetOpacity) {
        splShaderCtx.currentOpacity = Clamp(splShaderCtx.currentOpacity + deltaTime * 5.0f, SPL_OPACITY_MIN, splShaderCtx.targetOpacity);
    } else {
        splShaderCtx.currentOpacity = Clamp(splShaderCtx.currentOpacity - deltaTime * 5.0f, splShaderCtx.targetOpacity, SPL_OPACITY_MAX);
    }

    if (splShaderCtx.currentRadius < splShaderCtx.targetRadius) {
        splShaderCtx.currentRadius = Clamp(splShaderCtx.currentRadius + deltaTime * 500.0f, SPL_RADIUS_MIN, splShaderCtx.targetRadius);
    } else {
        splShaderCtx.currentRadius = Clamp(splShaderCtx.currentRadius - deltaTime * 500.0f, splShaderCtx.targetRadius, SPL_RADIUS_MAX);
    }

    splShaderCtx.center[0] = inputCtx.mousePos.x;
    splShaderCtx.center[1] = inputCtx.mousePos.y;
    splShaderCtx.textureWidth = renderWidth;
    splShaderCtx.textureHeight = renderHeight;
}

void setSpotlightShaderUniformValues(void) {
    SetShaderValue(splShader, splShaderLocCtx.opacity, &splShaderCtx.currentOpacity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(splShader, splShaderLocCtx.center, splShaderCtx.center, SHADER_UNIFORM_VEC2);
    SetShaderValue(splShader, splShaderLocCtx.radius, &splShaderCtx.currentRadius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(splShader, splShaderLocCtx.textureWidth, &splShaderCtx.textureWidth, SHADER_UNIFORM_INT);
    SetShaderValue(splShader, splShaderLocCtx.textureHeight, &splShaderCtx.textureHeight, SHADER_UNIFORM_INT);
}

void handleInput(void) {
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 targetDelta = {inputCtx.mouseDelta.x * (-1.0f / cameraCtx.camera.zoom),
                               inputCtx.mouseDelta.y * (-1.0f / cameraCtx.camera.zoom)};
        cameraCtx.camera.target = Vector2Add(cameraCtx.camera.target, targetDelta);
    }

    if (inputCtx.wheelDelta != 0.0f) {
        /* calculate scale factor */
        float scaleFactor = 1.0f + (0.25f * fabsf(inputCtx.wheelDelta));
        if (inputCtx.wheelDelta < 0) scaleFactor = 1.0f / scaleFactor;

        if (showSpotlight) {
            /* scroll to change spotlight radius */
            float radius = splShaderCtx.currentRadius * scaleFactor;
            if (radius >= SPL_RADIUS_MIN && radius <= SPL_RADIUS_MAX) {
                splShaderCtx.targetRadius = radius;
            }
        } else {
            /* scroll to zoom in & out */
            cameraCtx.camera.offset = inputCtx.mousePos;
            cameraCtx.camera.target = inputCtx.mouseWorldPos;

            float zoom = cameraCtx.camera.zoom * scaleFactor;
            if (zoom >= ZOOM_MIN && zoom <= ZOOM_MAX) {
                cameraCtx.targetZoom = zoom;
            }
        }
    }

    if (IsKeyPressed(KEY_H)) {
        showKeystrokeTips = !showKeystrokeTips;
    }

    if (IsKeyPressed(KEY_D)) {
        showDebugInfo = !showDebugInfo;
    }

    if (IsKeyPressed(KEY_L)) {
        showSpotlight = !showSpotlight;
        if (showSpotlight) {
            splShaderCtx.currentRadius = splShaderCtx.targetRadius + 100.0f;
            splShaderCtx.targetOpacity = SPL_OPACITY_MAX;
        } else {
            splShaderCtx.targetOpacity = SPL_OPACITY_MIN;
        }
    }
}

void drawDebugInfo(void) {
    DrawRectangle(10, 30, 500, 120, Fade(GRAY, 0.95f));
    DrawRectangleLinesEx((Rectangle){10, 30, 500, 120}, 2.0f, BLACK);
    DrawText(TextFormat("zoom: %f", cameraCtx.camera.zoom), 20, 40, 20, RAYWHITE);
    DrawText(TextFormat("camera offset: (%f, %f)", cameraCtx.camera.offset.x, cameraCtx.camera.offset.y), 20, 60, 20, RAYWHITE);
    DrawText(TextFormat("camera target: (%f, %f)", cameraCtx.camera.target.x, cameraCtx.camera.target.y), 20, 80, 20, RAYWHITE);
    DrawText(TextFormat("mouse position: (%f, %f)", inputCtx.mousePos.x, inputCtx.mousePos.y), 20, 100, 20, RAYWHITE);
    DrawText(TextFormat("mouse world position: (%f, %f)", inputCtx.mouseWorldPos.x, inputCtx.mouseWorldPos.y), 20, 120, 20, RAYWHITE);
}

void drawKeystrokeTips(void) {
    DrawRectangle(screenWidth - 310, 30, 300, 100, Fade(PINK, 0.95f));
    DrawRectangleLinesEx((Rectangle){screenWidth - 310, 30, 300, 100}, 2.0f, PURPLE);
    DrawText("esc - quit", screenWidth - 300, 40, 20, RAYWHITE);
    DrawText("h - toggle keystroke tips", screenWidth - 300, 60, 20, RAYWHITE);
    DrawText("d - toggle debug info", screenWidth - 300, 80, 20, RAYWHITE);
    DrawText("l - toggle spotlight", screenWidth - 300, 100, 20, RAYWHITE);
}

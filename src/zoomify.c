#include <limits.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__)
#include "macos_screenshot.h"
#elif defined(__linux__)
#include "linux_screenshot.h"
#endif

#define ZOOM_MIN ((float)0.01f)
#define ZOOM_MAX ((float)100.0f)
#define SPL_RADIUS_MIN ((float)1.0f)
#define SPL_RADIUS_MAX ((float)500.0f)

static int screenWidth, screenHeight;
static int renderWidth, renderHeight;

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
} inputContext = {0};

struct SpotlightShaderUniformLocationContext {
    int enable;
    int center;
    int radius;
    int textureWidth;
    int textureHeight;
} splShaderLocContext = {0};

struct SpotlightShaderUniformContext {
    int enable;
    float center[2];
    float radius;
    int textureWidth;
    int textureHeight;
} splShaderUniformContext = {.enable = false, .radius = 100.0f};

static Camera2D camera = {0};
static Shader splShader = {0};

void updateInputContext(void);
void updateSpotlightShaderUniformContext(void);
void updateSpotlightShaderUniformValue(void);
void handleInput(void);
void drawDebugInfo(void);
void drawKeystrokeTips(void);

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
    splShader = LoadShader("", "src/spotlight.glsl");
    /* get shader uniform location */
    splShaderLocContext.enable = GetShaderLocation(splShader, "enable");
    splShaderLocContext.center = GetShaderLocation(splShader, "center");
    splShaderLocContext.radius = GetShaderLocation(splShader, "radius");
    splShaderLocContext.textureWidth = GetShaderLocation(splShader, "textureWidth");
    splShaderLocContext.textureHeight = GetShaderLocation(splShader, "textureHeight");

    /* get resolution info */
    int currentMonitor = GetCurrentMonitor();
    screenWidth = GetMonitorWidth(currentMonitor);
    screenHeight = GetMonitorHeight(currentMonitor);
    renderWidth = GetRenderWidth();
    renderHeight = GetRenderHeight();
    /* calculate camera zoom */
    camera.zoom = fminf((float)renderWidth / (float)screenshotTexture.width, (float)renderHeight / (float)screenshotTexture.height);

    RenderTexture2D splMask = LoadRenderTexture(screenWidth, screenHeight);

    // clang-format off
    while(!WindowShouldClose()) {
        /* update input context */
        updateInputContext();

        /* handle user input */
        handleInput();

        /* update shader uniform context */
        updateSpotlightShaderUniformContext();

        /* update shader uniform value */
        updateSpotlightShaderUniformValue();

        /* rendering */
        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode2D(camera);
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

void updateInputContext(void) {
    inputContext.mousePos = GetMousePosition();
    inputContext.mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
    inputContext.mouseDelta = GetMouseDelta();
    inputContext.wheelDelta = GetMouseWheelMove();
}

void updateSpotlightShaderUniformContext(void) {
    splShaderUniformContext.center[0] = GetMouseX();
    splShaderUniformContext.center[1] = GetMouseY();
    splShaderUniformContext.textureWidth = GetRenderWidth();
    splShaderUniformContext.textureHeight = GetRenderHeight();
}

void updateSpotlightShaderUniformValue(void) {
    SetShaderValue(splShader, splShaderLocContext.enable, &splShaderUniformContext.enable, SHADER_UNIFORM_INT);
    SetShaderValue(splShader, splShaderLocContext.center, splShaderUniformContext.center, SHADER_UNIFORM_VEC2);
    SetShaderValue(splShader, splShaderLocContext.radius, &splShaderUniformContext.radius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(splShader, splShaderLocContext.textureWidth, &splShaderUniformContext.textureWidth, SHADER_UNIFORM_INT);
    SetShaderValue(splShader, splShaderLocContext.textureHeight, &splShaderUniformContext.textureHeight, SHADER_UNIFORM_INT);
}

void handleInput(void) {
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        camera.target.x += inputContext.mouseDelta.x * (-1.0f / camera.zoom);
        camera.target.y += inputContext.mouseDelta.y * (-1.0f / camera.zoom);
    }

    if (inputContext.wheelDelta != 0.0f) {
        /* calculate scale factor */
        float scaleFactor = 1.0f + (0.25f * fabsf(inputContext.wheelDelta));
        if (inputContext.wheelDelta < 0) scaleFactor = 1.0f / scaleFactor;

        if (splShaderUniformContext.enable) {
            /* scroll to change spotlight radius */
            float radius = splShaderUniformContext.radius * scaleFactor;
            if (radius > SPL_RADIUS_MIN && radius < SPL_RADIUS_MAX) {
                splShaderUniformContext.radius = radius;
            }
        } else {
            /* scroll to zoom in & out */
            camera.offset = inputContext.mousePos;
            camera.target = inputContext.mouseWorldPos;

            float zoom = camera.zoom * scaleFactor;
            if (zoom > ZOOM_MIN && zoom < ZOOM_MAX) {
                camera.zoom = zoom;
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
        splShaderUniformContext.enable = !splShaderUniformContext.enable;
    }
}

void drawDebugInfo(void) {
    DrawRectangle(10, 30, 500, 120, Fade(GRAY, 0.95f));
    DrawRectangleLinesEx((Rectangle){10, 30, 500, 120}, 2.0f, BLACK);
    DrawText(TextFormat("zoom: %f", camera.zoom), 20, 40, 20, RAYWHITE);
    DrawText(TextFormat("camera offset: (%f, %f)", camera.offset.x, camera.offset.y), 20, 60, 20, RAYWHITE);
    DrawText(TextFormat("camera target: (%f, %f)", camera.target.x, camera.target.y), 20, 80, 20, RAYWHITE);
    DrawText(TextFormat("mouse position: (%f, %f)", inputContext.mousePos.x, inputContext.mousePos.y), 20, 100, 20, RAYWHITE);
    DrawText(TextFormat("mouse world position: (%f, %f)", inputContext.mouseWorldPos.x, inputContext.mouseWorldPos.y), 20, 120, 20, RAYWHITE);
}

void drawKeystrokeTips(void) {
    DrawRectangle(screenWidth - 310, 30, 300, 100, Fade(PINK, 0.95f));
    DrawRectangleLinesEx((Rectangle){screenWidth - 310, 30, 300, 100}, 2.0f, PURPLE);
    DrawText("esc - quit", screenWidth - 300, 40, 20, RAYWHITE);
    DrawText("h - toggle keystroke tips", screenWidth - 300, 60, 20, RAYWHITE);
    DrawText("d - toggle debug info", screenWidth - 300, 80, 20, RAYWHITE);
    DrawText("l - toggle spotlight", screenWidth - 300, 100, 20, RAYWHITE);
}

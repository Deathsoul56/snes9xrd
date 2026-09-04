#pragma once

#include <cstdint>

namespace ImGui {
enum DrawTextAlignment
{
    BEGIN,
    END,
    CENTER
};
} // namespace ImGui

struct S9xImGuiInitInfo
{
    int font_size;
    int spacing;
    uint32_t box_color;
    uint32_t text_color;
    uint32_t inactive_text_color;
};

S9xImGuiInitInfo S9xImGuiGetDefaults();
void S9xImGuiInit(S9xImGuiInitInfo *init_info = nullptr);
bool S9xImGuiDraw(int width, int height);
bool S9xImGuiRunning();
void S9xImGuiDeinit();

// The canvas registers/replaces the backend-specific texture (an OpenGL
// texture name cast to void*, or a Vulkan ImGui descriptor set) here
// whenever GFX.InfoImageGeneration changes. texture_id = nullptr means no
// icon is drawn. Ownership/lifetime of the texture stays with the canvas.
void S9xImGuiSetAchievementBadgeTexture(void *texture_id, int width, int height);

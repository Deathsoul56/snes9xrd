#include "snes9x_imgui.h"
#include "snes9x_imgui_noto.h"
#include "imgui.h"

#include <cstdint>
#include <cstring>
#include <cfloat>
#include <string>
#include <array>
#include <algorithm>

#include "snes9x.h"
#include "port.h"
#include "controls.h"
#include "movie.h"
#include "gfx.h"
#include "ppu.h"
#include "cheats.h"

namespace
{
    S9xImGuiInitInfo settings;

    struct
    {
        void *texture_id = nullptr;
        int width = 0;
        int height = 0;
    } achievement_badge;
} // anonymous

void S9xImGuiSetAchievementBadgeTexture(void *texture_id, int width, int height)
{
    achievement_badge.texture_id = texture_id;
    achievement_badge.width = width;
    achievement_badge.height = height;
}

static void ImGui_DrawPressedKeys(int spacing)
{

    const std::array<const char *, 15> keynames =
        { " ", " ", " ", "R", "L", "X", "A", "→", "←", "↓", "↑", "St", "Sel", "Y", "B" };
    const std::array<int, 12> keyorder =
        { 10, 9, 8, 7, 6, 14, 13, 5, 4, 3, 11, 12 }; // < ^ > v   A B Y X  L R  S s

    enum controllers controller;
    int num_lines = 0;
    int cell_width = ImGui::CalcTextSize("→ ").x;
    int8_t ids[4];
    std::string final_string;

    auto draw_list = ImGui::GetForegroundDrawList();

    for (int port = 0; port < 2; port++)
    {
        S9xGetController(port, &controller, &ids[0], &ids[1], &ids[2], &ids[3]);
        if (controller == CTL_JOYPAD || controller == CTL_MOUSE)
            num_lines++;
    }

    if (num_lines == 0)
        return;

    for (int port = 0; port < 2; port++)
    {
        S9xGetController(port, &controller, &ids[0], &ids[1], &ids[2], &ids[3]);

        switch (controller)
        {
        case CTL_MOUSE: {
            uint8_t buf[5];
            char string[256];
            if (!MovieGetMouse(port, buf))
                break;
            int16_t mouse_x = READ_WORD(buf);
            int16_t mouse_y = READ_WORD(buf + 2);
            uint8_t buttons = buf[4];
            sprintf(string, "#%d %d: (%03d,%03d) %c%c", port + 1, ids[0] + 1, mouse_x, mouse_y,
                    (buttons & 0x40) ? 'L' : ' ', (buttons & 0x80) ? 'R' : ' ');

            auto string_size = ImGui::CalcTextSize(string);
            int box_width = 2 * spacing + string_size.x;
            int box_height = 2 * spacing + string_size.y;
            int x = (ImGui::GetIO().DisplaySize.x - box_width) / 2;
            int y = ImGui::GetIO().DisplaySize.y - (spacing + box_height) * num_lines;

            draw_list->AddRectFilled(ImVec2(x, y),
                                     ImVec2(x + box_width, y + box_height),
                                     settings.box_color,
                                     spacing / 2.0f);

            draw_list->AddText(ImVec2(x + spacing, y + spacing), settings.text_color, string);

            break;
        }

        case CTL_JOYPAD: {
            std::string prefix = "#" + std::to_string(port + 1) + " ";
            auto prefix_size = ImGui::CalcTextSize(prefix.c_str());
            int box_width = 2 * spacing + prefix_size.x + cell_width * keyorder.size();
            int box_height = 2 * spacing + prefix_size.y;
            int x = (ImGui::GetIO().DisplaySize.x - box_width) / 2;
            int y = ImGui::GetIO().DisplaySize.y - (spacing + box_height) * num_lines;

            draw_list->AddRectFilled(ImVec2(x, y),
                                     ImVec2(x + box_width, y + box_height),
                                     settings.box_color,
                                     spacing / 2.0f);
            x += spacing;
            y += spacing;

            draw_list->AddText(ImVec2(x, y), settings.text_color, prefix.c_str());
            x += prefix_size.x;

            uint16 pad = MovieGetJoypad(ids[0]);
            for (size_t i = 0; i < keyorder.size(); i++)
            {
                int j = keyorder[i];
                int mask = (1 << (j + 1));
                auto color = (pad & mask) ? settings.text_color : settings.inactive_text_color;
                draw_list->AddText(ImVec2(x, y), color, keynames[j]);
                x += cell_width;
            }

            num_lines--;
            break;
        }

        default:
            break;
        }
    }
}

static void ImGui_GetWatchesText(std::string& osd_text)
{
    for (unsigned int i = 0; i < sizeof(watches) / sizeof(watches[0]); i++)
    {
        if (!watches[i].on)
            break;

        int32	displayNumber = 0;
        char	buf[64];

        for (int r = 0; r < watches[i].size; r++)
            displayNumber += (Cheat.CWatchRAM[(watches[i].address - 0x7E0000) + r]) << (8 * r);

        if (watches[i].format == 1)
            sprintf(buf, "%s,%du = %u", watches[i].desc, watches[i].size, (unsigned int)displayNumber);
        else
            if (watches[i].format == 3)
                sprintf(buf, "%s,%dx = %X", watches[i].desc, watches[i].size, (unsigned int)displayNumber);
            else // signed
            {
                if (watches[i].size == 1)
                    displayNumber = (int32)((int8)displayNumber);
                else if (watches[i].size == 2)
                    displayNumber = (int32)((int16)displayNumber);
                else if (watches[i].size == 3)
                    if (displayNumber >= 8388608)
                        displayNumber -= 16777216;

                sprintf(buf, "%s,%ds = %d", watches[i].desc, watches[i].size, (int)displayNumber);
            }

        osd_text += buf;
        osd_text += '\n';
    }
}

static ImVec2 ImGui_DrawTextOverlay(const char *text,
                                  int x, int y,
                                  int padding,
                                  ImGui::DrawTextAlignment halign = ImGui::DrawTextAlignment::BEGIN,
                                  ImGui::DrawTextAlignment valign = ImGui::DrawTextAlignment::BEGIN,
                                  int wrap_at = 0,
                                  ImTextureID icon = nullptr,
                                  int icon_size = 0,
                                  bool bigger_title = false)
{
    ImFont *font = ImGui::GetFont();
    float title_font_size = font->FontSize * 1.2f;

    // DuckStation-style toast: first line (the game title) rendered bigger,
    // the rest of the message at the normal size.
    const char *title_end = bigger_title ? strchr(text, '\n') : nullptr;
    const char *body_begin = title_end ? title_end + 1 : text;

    int icon_extra = icon ? icon_size + padding : 0;
    float wrap_width = wrap_at ? (float)(wrap_at - icon_extra) : 0.0f;

    ImVec2 title_size(0.0f, 0.0f);
    if (title_end)
        title_size = font->CalcTextSizeA(title_font_size, FLT_MAX, wrap_width, text, title_end);
    ImVec2 body_size = font->CalcTextSizeA(font->FontSize, FLT_MAX, wrap_width, body_begin);

    auto text_size = ImVec2(std::max(title_size.x, body_size.x), title_size.y + body_size.y);
    auto box_size = ImVec2(text_size.x + icon_extra + padding * 2, std::max(text_size.y, (float)icon_size) + padding * 2);
    auto draw_list = ImGui::GetForegroundDrawList();
    if (halign == ImGui::DrawTextAlignment::END)
        x = x - box_size.x;
    else if (halign == ImGui::DrawTextAlignment::CENTER)
        x = x - box_size.x / 2;
    if (valign == ImGui::DrawTextAlignment::END)
        y = y - box_size.y;

    draw_list->AddRectFilled(ImVec2(x, y),
                             ImVec2(x + box_size.x, y + box_size.y),
                             settings.box_color,
                             settings.spacing / 2.0f);

    float text_x = x + padding;
    if (icon)
    {
        // Icon on the left edge of the box for left-anchored boxes, right
        // edge for right-anchored ones -- so it stays on the "outside" of
        // the message regardless of which screen corner it's drawn in.
        float icon_x = (halign == ImGui::DrawTextAlignment::END) ? x + box_size.x - padding - icon_size : x + padding;
        float icon_y = y + (box_size.y - icon_size) / 2.0f;
        draw_list->AddImage(icon, ImVec2(icon_x, icon_y), ImVec2(icon_x + icon_size, icon_y + icon_size));
        if (halign != ImGui::DrawTextAlignment::END)
            text_x += icon_extra;
    }

    float text_y = y + padding;
    if (title_end)
    {
        draw_list->AddText(font, title_font_size, ImVec2(text_x, text_y), settings.text_color, text, title_end, wrap_width);
        text_y += title_size.y;
    }
    draw_list->AddText(font, font->FontSize, ImVec2(text_x, text_y), settings.text_color, body_begin, nullptr, wrap_width);

    return box_size;
}

static std::string sjis_to_utf8(std::string in)
{
    std::string out;
    for (const auto &i : in)
    {
        unsigned char c = i;
        if (c > 160 && c < 192)
		{
            out += "\357\275";
			out += c;
		}
        else if (c >= 192)
        {
            out += "\357\276";
            c -= 0x40;
			out += c;
        }
        else
            out += c;
    }

    return out;
}

bool S9xImGuiDraw(int width, int height)
{
    if (Memory.ROMFilename.empty())
        return false;

    if (!ImGui::GetCurrentContext())
        return false;

    ImGui::GetIO().DisplaySize.x = width;
    ImGui::GetIO().DisplaySize.y = height;
    ImGui::GetIO().DisplayFramebufferScale.x = 1.0;
    ImGui::GetIO().DisplayFramebufferScale.y = 1.0;
    ImGui::NewFrame();

    if (Settings.DisplayTime)
    {
        char string[256];

        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        sprintf(string, "%02u:%02u", timeinfo->tm_hour, timeinfo->tm_min);
        ImGui_DrawTextOverlay(string,
                              width - settings.spacing,
                              height - settings.spacing,
                              settings.spacing,
                              ImGui::DrawTextAlignment::END,
                              ImGui::DrawTextAlignment::END);
    }

    if (Settings.DisplayFrameRate)
    {
        char string[256];
        static uint32 lastFrameCount = 0, calcFps = 0;
        static time_t lastTime = time(NULL);

        time_t currTime = time(NULL);
        if (lastTime != currTime)
        {
            if (lastFrameCount < IPPU.TotalEmulatedFrames)
            {
                calcFps = (IPPU.TotalEmulatedFrames - lastFrameCount) / (uint32)(currTime - lastTime);
            }
            lastTime = currTime;
            lastFrameCount = IPPU.TotalEmulatedFrames;
        }

        sprintf(string, "%u fps\n%02d/%02d", calcFps, (int)IPPU.DisplayedRenderedFrameCount, (int)Memory.ROMFramesPerSecond);

        ImGui_DrawTextOverlay(string,
                              width - settings.spacing,
                              settings.spacing,
                              settings.spacing,
                              ImGui::DrawTextAlignment::END,
                              ImGui::DrawTextAlignment::BEGIN);
    }

    if (Settings.DisplayPressedKeys)
    {
        ImGui_DrawPressedKeys(settings.spacing / 2);
    }

    if (Settings.DisplayIndicators)
    {
        if (Settings.Paused || Settings.ForcedPause)
        {
            ImGui_DrawTextOverlay("❚❚",
                                  settings.spacing,
                                  settings.spacing,
                                  settings.spacing);
        }
        else if (Settings.TurboMode)
        {
            ImGui_DrawTextOverlay("▶▶",
                                  settings.spacing,
                                  settings.spacing,
                                  settings.spacing);
        }
    }

    std::string utf8_message;
    if (Settings.DisplayWatchedAddresses)
    {
        ImGui_GetWatchesText(utf8_message);
    }

    if (Settings.DisplayMovieFrame && S9xMovieActive())
    {
        if (!utf8_message.empty() && utf8_message.back() != '\n')
        {
            utf8_message += '\n';
        }
        utf8_message += GFX.FrameDisplayString;
    }

    if (!utf8_message.empty())
    {
        ImGui_DrawTextOverlay(utf8_message.c_str(),
                              settings.spacing,
                              height - settings.spacing,
                              settings.spacing,
                              ImGui::DrawTextAlignment::BEGIN,
                              ImGui::DrawTextAlignment::END,
                              width - settings.spacing * 4);
    }

    if (!GFX.InfoMessages.empty())
    {
        // Same four corners as the "Inscreen" bitmap-font renderer
        // (Settings.InfoStringLocation, see gfx.cpp's S9xDisplayMessages).
        bool right = (Settings.InfoStringLocation == 1 || Settings.InfoStringLocation == 3);
        bool top = (Settings.InfoStringLocation == 2 || Settings.InfoStringLocation == 3);

        int x = right ? width - settings.spacing : settings.spacing;
        int y = top ? settings.spacing : height - settings.spacing;
        int gap = settings.spacing / 2;

        // Newest message nearest the anchor corner (where a single message
        // used to sit); older ones stacked further away and pushed along as
        // new messages arrive, instead of being wiped out instantly.
        for (auto it = GFX.InfoMessages.rbegin(); it != GFX.InfoMessages.rend(); ++it)
        {
            ImTextureID icon = nullptr;
            int icon_size = 0;
            if (it == GFX.InfoMessages.rbegin() && achievement_badge.texture_id && GFX.InfoImageWidth > 0)
            {
                icon = achievement_badge.texture_id;
                icon_size = settings.font_size * 2;
            }

            ImVec2 box_size = ImGui_DrawTextOverlay(sjis_to_utf8(it->text).c_str(),
                                  x, y,
                                  settings.spacing,
                                  right ? ImGui::DrawTextAlignment::END : ImGui::DrawTextAlignment::BEGIN,
                                  top ? ImGui::DrawTextAlignment::BEGIN : ImGui::DrawTextAlignment::END,
                                  width - settings.spacing * 4,
                                  icon, icon_size,
                                  /*bigger_title=*/true);

            y += top ? (box_size.y + gap) : -(box_size.y + gap);
        }
    }

    ImGui::Render();

    return true;
}

bool S9xImGuiRunning()
{
    if (ImGui::GetCurrentContext())
        return true;
    return false;
}

void S9xImGuiDeinit()
{
    if (S9xImGuiRunning())
        ImGui::DestroyContext();
}

S9xImGuiInitInfo S9xImGuiGetDefaults()
{
    return { 24, 10, 0x88000000, 0xffffffff, 0x44ffffff };
}

void S9xImGuiInit(S9xImGuiInitInfo *init_info)
{
    static ImVector<ImWchar> ranges;
    if (ImGui::GetCurrentContext())
        return;

    if (init_info)
    {
        ::settings = *init_info;
    }
    else
    {
        settings = S9xImGuiGetDefaults();
    }

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    ImGui::StyleColorsLight();
    ImFontGlyphRangesBuilder builder;
    builder.Clear();
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());
    builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesJapanese());
    builder.AddText("←↑→↓▶❚");
    ranges.clear();
    builder.BuildRanges(&ranges);
    ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(imgui_noto_font_compressed_data, imgui_noto_font_compressed_size, settings.font_size, nullptr, ranges.Data);
}

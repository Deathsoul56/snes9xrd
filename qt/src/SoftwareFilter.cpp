#include "SoftwareFilter.hpp"
#include "../port.h"
#include "filter/2xsai.h"
#include "filter/epx.h"
#include "filter/hq2x.h"
#include "filter/xbrz.h"
#include "filter/xbrz_tools.h"
#include "filter/snes_ntsc.h"
#include <cstring>

namespace
{

using FilterFunc = void (*)(uint8_t *, int, uint8_t *, int, int, int);

void simpleScale(const uint16_t *src, int src_pitch, uint16_t *dst, int dst_pitch, int width, int height, int scale)
{
    for (int y = 0; y < height; y++)
    {
        auto in = (const uint16_t *)((const uint8_t *)src + y * src_pitch);
        auto out = (uint16_t *)((uint8_t *)dst + (y * scale) * dst_pitch);

        for (int x = 0; x < width; x++)
            for (int s = 0; s < scale; s++)
                out[x * scale + s] = in[x];

        for (int line = 1; line < scale; line++)
            memcpy((uint8_t *)dst + (y * scale + line) * dst_pitch,
                   (uint8_t *)dst + (y * scale) * dst_pitch,
                   width * scale * 2);
    }
}

void simple2x(uint8_t *src, int sp, uint8_t *dst, int dp, int w, int h) { simpleScale((const uint16_t *)src, sp, (uint16_t *)dst, dp, w, h, 2); }
void simple3x(uint8_t *src, int sp, uint8_t *dst, int dp, int w, int h) { simpleScale((const uint16_t *)src, sp, (uint16_t *)dst, dp, w, h, 3); }
void simple4x(uint8_t *src, int sp, uint8_t *dst, int dp, int w, int h) { simpleScale((const uint16_t *)src, sp, (uint16_t *)dst, dp, w, h, 4); }

// A soft scanline overlay: draws each source line twice, dimming the copy in
// between to emulate the gaps between an interlaced CRT's scanlines.
void scanlines(uint8_t *src, int src_pitch, uint8_t *dst, int dst_pitch, int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        auto in = (const uint16_t *)(src + y * src_pitch);
        auto out_a = (uint16_t *)(dst + (y * 2) * dst_pitch);
        auto out_b = (uint16_t *)(dst + (y * 2 + 1) * dst_pitch);

        for (int x = 0; x < width; x++)
        {
            uint16_t pixel = in[x];
            out_a[x] = pixel;
            out_b[x] = pixel - ((pixel >> 1) & 0x738E); // dim ~50%, RGB565-safe
        }
    }
}

struct FilterInfo
{
    const char *name;
    FilterFunc func; // nullptr for filters handled specially below (xBRZ, NTSC)
    int xscale;
    int yscale;
};

const FilterInfo &filterInfo(int type)
{
    static const FilterInfo table[] = {
        { "None",        nullptr,    1, 1 },
        { "Scanlines",   scanlines,  1, 2 },
        { "Simple 2x",   simple2x,   2, 2 },
        { "Simple 3x",   simple3x,   3, 3 },
        { "Simple 4x",   simple4x,   4, 4 },
        { "Super Eagle", SuperEagle, 2, 2 },
        { "2xSaI",       _2xSaI,     2, 2 },
        { "Super 2xSaI", Super2xSaI, 2, 2 },
        { "EPX",         EPX_16,     2, 2 },
        { "HQ2x",        HQ2X_16,    2, 2 },
        { "HQ3x",        HQ3X_16,    3, 3 },
        { "HQ4x",        HQ4X_16,    4, 4 },
        { "2xBRZ",       nullptr,    2, 2 },
        { "3xBRZ",       nullptr,    3, 3 },
        { "4xBRZ",       nullptr,    4, 4 },
        { "NTSC",        nullptr,    1, 2 }, // width handled via SNES_NTSC_OUT_WIDTH
    };
    return table[type];
}

} // namespace

const std::vector<const char *> &SoftwareFilter::names()
{
    static const std::vector<const char *> list = [] {
        std::vector<const char *> v;
        for (int i = EmuConfig::eFilterNone; i <= EmuConfig::eFilterNTSC; i++)
            v.push_back(filterInfo(i).name);
        return v;
    }();
    return list;
}

SoftwareFilter::SoftwareFilter()
{
    S9xBlit2xSaIFilterInit();
}
SoftwareFilter::~SoftwareFilter() = default;

void SoftwareFilter::applyXBRZ(int factor, const uint16_t *src, int src_pitch, int width, int height,
                                uint16_t *dst, int dst_pitch)
{
    xbrz_src.resize((size_t)width * height);
    xbrz_dst.resize((size_t)width * factor * height * factor);

    for (int y = 0; y < height; y++)
    {
        auto in = (const uint16_t *)((const uint8_t *)src + y * src_pitch);
        for (int x = 0; x < width; x++)
            xbrz_src[(size_t)y * width + x] = xbrz::rgb565to888(in[x]);
    }

    // ponytail: single-threaded xBRZ scale, upgrade to a worker pool if
    // profiling shows 3x/4xBRZ at high internal resolutions costs real frame time.
    xbrz::scale(factor, xbrz_src.data(), xbrz_dst.data(), width, height, xbrz::ColorFormat::RGB);

    int out_width = width * factor;
    for (int y = 0; y < height * factor; y++)
    {
        auto out = (uint16_t *)((uint8_t *)dst + y * dst_pitch);
        auto in_row = &xbrz_dst[(size_t)y * out_width];
        for (int x = 0; x < out_width; x++)
            out[x] = xbrz::rgb888to565(in_row[x]);
    }
}

void SoftwareFilter::applyNTSC(const uint16_t *src, int src_pitch, int width, int height,
                                uint16_t *dst, int dst_pitch)
{
    if (!ntsc)
    {
        ntsc = std::make_unique<snes_ntsc_t>();
        snes_ntsc_init(ntsc.get(), &snes_ntsc_composite);
    }

    if (width > 256)
        snes_ntsc_blit_hires_scanlines(ntsc.get(), (SNES_NTSC_IN_T *)src, src_pitch >> 1,
                                        burst_phase, width, height, dst, dst_pitch);
    else
        snes_ntsc_blit_scanlines(ntsc.get(), (SNES_NTSC_IN_T *)src, src_pitch >> 1,
                                  burst_phase, width, height, dst, dst_pitch);

    burst_phase = (burst_phase + 1) % 3;
}

bool SoftwareFilter::apply(int type,
                           const uint16_t *src, int src_pitch, int width, int height,
                           const uint16_t *&out_data, int &out_width, int &out_height, int &out_pitch)
{
    if (type <= EmuConfig::eFilterNone || type > EmuConfig::eFilterNTSC)
        return false;

    const auto &info = filterInfo(type);

    if (type == EmuConfig::eFilterNTSC)
    {
        out_width = SNES_NTSC_OUT_WIDTH(256);
        out_height = height * info.yscale;
    }
    else
    {
        out_width = width * info.xscale;
        out_height = height * info.yscale;
    }
    out_pitch = out_width * 2;

    scratch.resize((size_t)out_pitch * out_height);
    auto dst = (uint16_t *)scratch.data();

    if (type == EmuConfig::eFilterNTSC)
    {
        applyNTSC(src, src_pitch, width, height, dst, out_pitch);
    }
    else if (type == EmuConfig::eFilterTwoXBRZ || type == EmuConfig::eFilterThreeXBRZ || type == EmuConfig::eFilterFourXBRZ)
    {
        applyXBRZ(info.xscale, src, src_pitch, width, height, dst, out_pitch);
    }
    else
    {
        info.func((uint8_t *)src, src_pitch, (uint8_t *)dst, out_pitch, width, height);
    }

    out_data = dst;
    return true;
}

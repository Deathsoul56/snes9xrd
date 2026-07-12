#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include "EmuConfig.hpp"

struct snes_ntsc_t;

// Applies an optional CPU-side pixel-art upscaler or NTSC filter to the raw
// RGB565 framebuffer the core produces, before it reaches any of the Qt
// display backends (Qt Software/OpenGL/Vulkan). This is the single point
// where all three backends receive already-filtered pixels, so none of them
// need to implement filtering themselves.
class SoftwareFilter
{
  public:
    SoftwareFilter();
    ~SoftwareFilter();

    // Display names for populating a combo box, in EmuConfig::SoftwareFilterType order.
    static const std::vector<const char *> &names();

    // Applies the filter selected by `type` to `src` (RGB565, `src_pitch` bytes per row).
    // Returns false if `type` is eFilterNone; the caller should keep using the
    // original buffer in that case. Otherwise out_data/out_width/out_height/out_pitch
    // describe the filtered image, valid until the next call to apply().
    bool apply(int type,
               const uint16_t *src, int src_pitch, int width, int height,
               const uint16_t *&out_data, int &out_width, int &out_height, int &out_pitch);

  private:
    void applyNTSC(const uint16_t *src, int src_pitch, int width, int height,
                   uint16_t *dst, int dst_pitch);
    void applyXBRZ(int factor, const uint16_t *src, int src_pitch, int width, int height,
                   uint16_t *dst, int dst_pitch);

    std::vector<uint8_t> scratch;
    std::vector<uint32_t> xbrz_src;
    std::vector<uint32_t> xbrz_dst;
    std::unique_ptr<snes_ntsc_t> ntsc;
    int burst_phase = 0;
};

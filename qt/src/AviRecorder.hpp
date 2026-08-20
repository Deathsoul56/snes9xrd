#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

class AviRecorder
{
  public:
    ~AviRecorder();

    bool start(const std::string &filename, int sample_rate, int channels, bool high_resolution);
    void addVideoFrame(const uint16_t *pixels, int width, int height, int stride_bytes, double frame_rate);
    void addAudioSamples(const int16_t *data, int samples);

    void stop();
    bool isActive() const { return file_ != nullptr; }

  private:
    void writeVideoHeader(int width, int height, double frame_rate);
    void patchDword(long pos, uint32_t value);

    FILE *file_ = nullptr;
    bool header_written_ = false;

    int width_ = 0;
    int height_ = 0;
    int sample_rate_ = 0;
    int channels_ = 0;
    int block_align_ = 0;
    bool high_resolution_ = false;

    long pos_riff_size_ = 0;
    long pos_hdrl_size_ = 0;
    long pos_movi_size_ = 0;
    long pos_avih_maxbytespersec_ = 0;
    long pos_avih_totalframes_ = 0;
    long pos_strh_vids_length_ = 0;
    long pos_strh_auds_length_ = 0;
    long movi_data_start_ = 0;

    uint32_t frame_count_ = 0;
    uint32_t audio_block_count_ = 0;

    struct IndexEntry
    {
        char fourcc[4];
        uint32_t flags;
        uint32_t offset;
        uint32_t size;
    };
    std::vector<IndexEntry> index_;
    std::vector<uint8_t> bgr_buffer_;
};

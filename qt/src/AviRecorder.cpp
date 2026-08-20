#include "AviRecorder.hpp"

#include <cstring>

namespace
{
constexpr uint32_t kAviIfKeyframe = 0x10;

void writeU32(FILE *f, uint32_t v) { fwrite(&v, sizeof(v), 1, f); }
void writeU16(FILE *f, uint16_t v) { fwrite(&v, sizeof(v), 1, f); }
void writeFourCC(FILE *f, const char cc[4]) { fwrite(cc, 1, 4, f); }
} // namespace

AviRecorder::~AviRecorder()
{
    stop();
}

bool AviRecorder::start(const std::string &filename, int sample_rate, int channels, bool high_resolution)
{
    stop();

    file_ = fopen(filename.c_str(), "wb");
    if (!file_)
        return false;

    sample_rate_ = sample_rate;
    channels_ = channels;
    block_align_ = channels_ * (int)sizeof(int16_t);
    high_resolution_ = high_resolution;
    header_written_ = false;
    frame_count_ = 0;
    audio_block_count_ = 0;
    index_.clear();

    return true;
}

void AviRecorder::patchDword(long pos, uint32_t value)
{
    long cur = ftell(file_);
    fseek(file_, pos, SEEK_SET);
    writeU32(file_, value);
    fseek(file_, cur, SEEK_SET);
}

void AviRecorder::writeVideoHeader(int width, int height, double frame_rate)
{
    width_ = width;
    height_ = height;

    uint32_t usec_per_frame = frame_rate > 0.0 ? (uint32_t)(1000000.0 / frame_rate) : 16667;
    uint32_t avg_bytes_per_sec = (uint32_t)sample_rate_ * (uint32_t)block_align_;

    writeFourCC(file_, "RIFF");
    pos_riff_size_ = ftell(file_);
    writeU32(file_, 0);
    writeFourCC(file_, "AVI ");

    writeFourCC(file_, "LIST");
    pos_hdrl_size_ = ftell(file_);
    writeU32(file_, 0);
    writeFourCC(file_, "hdrl");

    writeFourCC(file_, "avih");
    writeU32(file_, 56);
    writeU32(file_, usec_per_frame);
    pos_avih_maxbytespersec_ = ftell(file_);
    writeU32(file_, 0); // dwMaxBytesPerSec, patched in stop()
    writeU32(file_, 0); // dwPaddingGranularity
    writeU32(file_, 0x00000110); // AVIF_HASINDEX | AVIF_ISINTERLEAVED
    pos_avih_totalframes_ = ftell(file_);
    writeU32(file_, 0); // dwTotalFrames, patched
    writeU32(file_, 0); // dwInitialFrames
    writeU32(file_, 2); // dwStreams
    writeU32(file_, 0); // dwSuggestedBufferSize
    writeU32(file_, (uint32_t)width);
    writeU32(file_, (uint32_t)height);
    writeU32(file_, 0);
    writeU32(file_, 0);
    writeU32(file_, 0);
    writeU32(file_, 0); // dwReserved[4]

    writeFourCC(file_, "LIST");
    long strl_vids_size_pos = ftell(file_);
    writeU32(file_, 0);
    writeFourCC(file_, "strl");

    writeFourCC(file_, "strh");
    writeU32(file_, 56);
    writeFourCC(file_, "vids");
    writeFourCC(file_, "DIB ");
    writeU32(file_, 0); // dwFlags
    writeU16(file_, 0);
    writeU16(file_, 0); // priority, language
    writeU32(file_, 0); // dwInitialFrames
    writeU32(file_, 1000); // dwScale
    writeU32(file_, (uint32_t)(frame_rate > 0.0 ? frame_rate * 1000.0 : 60000.0)); // dwRate
    writeU32(file_, 0); // dwStart
    pos_strh_vids_length_ = ftell(file_);
    writeU32(file_, 0); // dwLength, patched
    writeU32(file_, 0); // dwSuggestedBufferSize
    writeU32(file_, 0); // dwQuality
    writeU32(file_, 0); // dwSampleSize
    writeU16(file_, 0);
    writeU16(file_, 0); // rcFrame left, top
    writeU16(file_, (uint16_t)width);
    writeU16(file_, (uint16_t)height); // rcFrame right, bottom

    writeFourCC(file_, "strf");
    writeU32(file_, 40);
    writeU32(file_, 40); // biSize
    writeU32(file_, (uint32_t)width);
    writeU32(file_, (uint32_t)-height); // negative -> top-down DIB rows
    writeU16(file_, 1); // biPlanes
    writeU16(file_, 24); // biBitCount
    writeU32(file_, 0); // biCompression = BI_RGB
    writeU32(file_, (uint32_t)width * (uint32_t)height * 3); // biSizeImage
    writeU32(file_, 0);
    writeU32(file_, 0);
    writeU32(file_, 0);
    writeU32(file_, 0);

    long strl_vids_end = ftell(file_);
    patchDword(strl_vids_size_pos, (uint32_t)(strl_vids_end - strl_vids_size_pos - 4));

    writeFourCC(file_, "LIST");
    long strl_auds_size_pos = ftell(file_);
    writeU32(file_, 0);
    writeFourCC(file_, "strl");

    writeFourCC(file_, "strh");
    writeU32(file_, 56);
    writeFourCC(file_, "auds");
    writeU32(file_, 0); // fccHandler
    writeU32(file_, 0); // dwFlags
    writeU16(file_, 0);
    writeU16(file_, 0); // priority, language
    writeU32(file_, 0); // dwInitialFrames
    writeU32(file_, (uint32_t)block_align_); // dwScale
    writeU32(file_, avg_bytes_per_sec); // dwRate
    writeU32(file_, 0); // dwStart
    pos_strh_auds_length_ = ftell(file_);
    writeU32(file_, 0); // dwLength, patched
    writeU32(file_, 0); // dwSuggestedBufferSize
    writeU32(file_, 0); // dwQuality
    writeU32(file_, (uint32_t)block_align_); // dwSampleSize
    writeU16(file_, 0);
    writeU16(file_, 0);
    writeU16(file_, 0);
    writeU16(file_, 0); // rcFrame

    writeFourCC(file_, "strf");
    writeU32(file_, 18);
    writeU16(file_, 1); // wFormatTag = WAVE_FORMAT_PCM
    writeU16(file_, (uint16_t)channels_);
    writeU32(file_, (uint32_t)sample_rate_);
    writeU32(file_, avg_bytes_per_sec);
    writeU16(file_, (uint16_t)block_align_);
    writeU16(file_, 16); // wBitsPerSample
    writeU16(file_, 0); // cbSize

    long strl_auds_end = ftell(file_);
    patchDword(strl_auds_size_pos, (uint32_t)(strl_auds_end - strl_auds_size_pos - 4));

    long hdrl_end = ftell(file_);
    patchDword(pos_hdrl_size_, (uint32_t)(hdrl_end - pos_hdrl_size_ - 4));

    writeFourCC(file_, "LIST");
    pos_movi_size_ = ftell(file_);
    writeU32(file_, 0);
    writeFourCC(file_, "movi");
    movi_data_start_ = ftell(file_);

    header_written_ = true;
}

void AviRecorder::addVideoFrame(const uint16_t *pixels, int width, int height, int stride_bytes, double frame_rate)
{
    if (!file_)
        return;

    int target_width = high_resolution_ || width <= 256 ? width : width / 2;
    int target_height = high_resolution_ || height <= 239 ? height : height / 2;
    if (!header_written_)
        writeVideoHeader(target_width, target_height, frame_rate);

    if (target_width != width_ || target_height != height_)
        return;

    bgr_buffer_.resize((size_t)width_ * (size_t)height_ * 3);
    uint8_t *out = bgr_buffer_.data();
    int stride_pixels = stride_bytes / (int)sizeof(uint16_t);
    int x_scale = width / width_;
    int y_scale = height / height_;

    for (int y = 0; y < height_; y++)
    {
        for (int x = 0; x < width_; x++)
        {
            int red = 0, green = 0, blue = 0;
            for (int source_y = 0; source_y < y_scale; source_y++)
            {
                const uint16_t *row = pixels + (size_t)(y * y_scale + source_y) * stride_pixels;
                for (int source_x = 0; source_x < x_scale; source_x++)
                {
                    uint16_t pixel = row[x * x_scale + source_x];
                    red += (pixel >> 11) & 0x1F;
                    green += (pixel >> 5) & 0x3F;
                    blue += pixel & 0x1F;
                }
            }
            int sample_count = x_scale * y_scale;
            uint8_t r5 = red / sample_count;
            uint8_t g6 = green / sample_count;
            uint8_t b5 = blue / sample_count;
            *out++ = (uint8_t)((b5 << 3) | (b5 >> 2));
            *out++ = (uint8_t)((g6 << 2) | (g6 >> 4));
            *out++ = (uint8_t)((r5 << 3) | (r5 >> 2));
        }
    }

    uint32_t size = (uint32_t)bgr_buffer_.size();
    long chunk_pos = ftell(file_);
    writeFourCC(file_, "00dc");
    writeU32(file_, size);
    fwrite(bgr_buffer_.data(), 1, size, file_);
    if (size & 1)
        fputc(0, file_);

    IndexEntry entry;
    memcpy(entry.fourcc, "00dc", 4);
    entry.flags = kAviIfKeyframe;
    entry.offset = (uint32_t)(chunk_pos - movi_data_start_);
    entry.size = size;
    index_.push_back(entry);

    frame_count_++;
}

void AviRecorder::addAudioSamples(const int16_t *data, int samples)
{
    if (!file_ || !header_written_ || samples <= 0)
        return;

    uint32_t size = (uint32_t)samples * (uint32_t)sizeof(int16_t);
    long chunk_pos = ftell(file_);
    writeFourCC(file_, "01wb");
    writeU32(file_, size);
    fwrite(data, 1, size, file_);
    if (size & 1)
        fputc(0, file_);

    IndexEntry entry;
    memcpy(entry.fourcc, "01wb", 4);
    entry.flags = kAviIfKeyframe;
    entry.offset = (uint32_t)(chunk_pos - movi_data_start_);
    entry.size = size;
    index_.push_back(entry);

    if (block_align_ > 0)
        audio_block_count_ += size / (uint32_t)block_align_;
}

void AviRecorder::stop()
{
    if (!file_)
        return;

    if (header_written_)
    {
        long movi_end = ftell(file_);
        patchDword(pos_movi_size_, (uint32_t)(movi_end - pos_movi_size_ - 4));

        writeFourCC(file_, "idx1");
        writeU32(file_, (uint32_t)(index_.size() * 16));
        for (auto &e : index_)
        {
            writeFourCC(file_, e.fourcc);
            writeU32(file_, e.flags);
            writeU32(file_, e.offset);
            writeU32(file_, e.size);
        }

        long file_end = ftell(file_);
        patchDword(pos_riff_size_, (uint32_t)(file_end - pos_riff_size_ - 4));

        patchDword(pos_avih_totalframes_, frame_count_);
        patchDword(pos_strh_vids_length_, frame_count_);
        patchDword(pos_strh_auds_length_, audio_block_count_);

        uint32_t video_bytes_per_sec = (uint32_t)width_ * (uint32_t)height_ * 3;
        patchDword(pos_avih_maxbytespersec_, video_bytes_per_sec + (uint32_t)sample_rate_ * (uint32_t)block_align_);
    }

    fclose(file_);
    file_ = nullptr;
    header_written_ = false;
    index_.clear();
}

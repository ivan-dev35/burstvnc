#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>
#include "proto.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace bst {

class Enc {
public:
    using Cb = std::function<void(const HdrVid& h, const uint8_t* d, size_t sz)>;

    Enc(int w, int h, int fps, int br, const std::string& pref = "auto");
    ~Enc();

    bool init();
    void feed(const uint8_t* bgra, int stride, uint64_t ts);
    void setCb(Cb cb) { cb_ = cb; }
    void reqIdr();
    void setBr(int br);

    const std::string& getCdc() const { return cdc_; }
    int getW() const { return w_; }
    int getH() const { return h_; }
    int getFps() const { return fps_; }
    int getBr() const { return br_; }

private:
    bool openCdc(const char* name);
    void toNv12(const uint8_t* bgra, int stride);
    void toYuv(const uint8_t* bgra, int stride);

    int w_;
    int h_;
    int fps_;
    int br_;
    std::string pref_;
    std::string cdc_;

    const AVCodec* c_ = nullptr;
    AVCodecContext* ctx_ = nullptr;
    AVFrame* frm_ = nullptr;
    AVPacket* pkt_ = nullptr;
    SwsContext* sws_ = nullptr;

    Cb cb_;
    std::atomic<bool> idr_{false};
    uint32_t seq_ = 0;
};

}

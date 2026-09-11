#pragma once
#include <vector>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <opus/opus.h>
#include "proto.hpp"

namespace bst {

class Aud {
public:
    using Cb = std::function<void(const HdrAud& h, const uint8_t* d, size_t sz)>;

    Aud(int rate = 48000, int ch = 2);
    ~Aud();

    bool init(const char* dev = nullptr);
    void start();
    void stop();
    void setCb(Cb cb) { cb_ = cb; }

private:
    void loop();

    int rate_;
    int ch_;
    int fsz_;
    std::atomic<bool> run_{false};
    std::thread th_;

    pa_simple* pa_ = nullptr;
    OpusEncoder* enc_ = nullptr;
    Cb cb_;
};

}

#include "aud.hpp"
#include <chrono>

namespace bst {

Aud::Aud(int rate, int ch)
    : rate_(rate), ch_(ch), fsz_(rate / 50) {}

Aud::~Aud() {
    stop();
    if (enc_) {
        opus_encoder_destroy(enc_);
        enc_ = nullptr;
    }
    if (pa_) {
        pa_simple_free(pa_);
        pa_ = nullptr;
    }
}

bool Aud::init(const char* dev) {
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = rate_;
    ss.channels = ch_;

    pa_buffer_attr ba;
    ba.maxlength = (uint32_t)-1;
    ba.tlength = (uint32_t)-1;
    ba.prebuf = (uint32_t)-1;
    ba.minreq = (uint32_t)-1;
    ba.fragsize = fsz_ * ch_ * sizeof(int16_t);

    int err;
    pa_ = pa_simple_new(nullptr, "BurstAud", PA_STREAM_RECORD,
                        dev ? dev : "remote_sink.monitor",
                        "Aud", &ss, nullptr, &ba, &err);
    if (!pa_) {
        pa_ = pa_simple_new(nullptr, "BurstAud", PA_STREAM_RECORD,
                            nullptr, "Aud", &ss, nullptr, &ba, &err);
    }
    if (!pa_) return false;

    int oerr;
    enc_ = opus_encoder_create(rate_, ch_, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &oerr);
    if (oerr != OPUS_OK || !enc_) return false;

    opus_encoder_ctl(enc_, OPUS_SET_BITRATE(96000));
    opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(3));
    opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    return true;
}

void Aud::start() {
    if (!pa_ || !enc_ || run_) return;
    run_ = true;
    th_ = std::thread(&Aud::loop, this);
}

void Aud::stop() {
    if (run_) {
        run_ = false;
        if (th_.joinable()) th_.join();
    }
}

void Aud::loop() {
    std::vector<int16_t> pcm(fsz_ * ch_);
    std::vector<uint8_t> obuf(4000);

    while (run_) {
        int err;
        if (pa_simple_read(pa_, pcm.data(), pcm.size() * sizeof(int16_t), &err) < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        auto ts = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        int nb = opus_encode(enc_, pcm.data(), fsz_, obuf.data(), obuf.size());
        if (nb > 0 && cb_) {
            HdrAud h;
            h.t = M_AUD;
            h.sz = nb;
            h.ts = ts;
            cb_(h, obuf.data(), nb);
        }
    }
}

}

#include "enc.hpp"
#include <iostream>
#include <thread>

namespace bst {

Enc::Enc(int w, int h, int fps, int br, const std::string& pref)
    : w_(w), h_(h), fps_(fps), br_(br), pref_(pref) {}

Enc::~Enc() {
    if (sws_) sws_freeContext(sws_);
    if (pkt_) av_packet_free(&pkt_);
    if (frm_) av_frame_free(&frm_);
    if (ctx_) avcodec_free_context(&ctx_);
}

bool Enc::init() {
    if (pref_ == "nvenc") {
        if (openCdc("h264_nvenc")) {
            cdc_ = "h264_nvenc";
            return true;
        }
        return false;
    } else if (pref_ == "x264") {
        if (openCdc("libx264")) {
            cdc_ = "libx264";
            return true;
        }
        return false;
    }

    if (openCdc("h264_nvenc")) {
        cdc_ = "h264_nvenc";
        return true;
    }
    if (openCdc("h264_vaapi")) {
        cdc_ = "h264_vaapi";
        return true;
    }
    if (openCdc("libx264")) {
        cdc_ = "libx264";
        return true;
    }
    return false;
}

bool Enc::openCdc(const char* name) {
    if (ctx_) {
        avcodec_free_context(&ctx_);
        ctx_ = nullptr;
    }

    c_ = avcodec_find_encoder_by_name(name);
    if (!c_) return false;

    ctx_ = avcodec_alloc_context3(c_);
    if (!ctx_) return false;

    ctx_->width = w_;
    ctx_->height = h_;
    ctx_->time_base = (AVRational){1, fps_};
    ctx_->framerate = (AVRational){fps_, 1};
    ctx_->bit_rate = (int64_t)br_ * 1000;
    ctx_->rc_max_rate = ctx_->bit_rate;
    ctx_->rc_buffer_size = ctx_->bit_rate / 30;
    ctx_->gop_size = fps_ * 2;
    ctx_->max_b_frames = 0;
    ctx_->thread_count = 4;

    if (std::string(name) == "h264_nvenc") {
        ctx_->pix_fmt = AV_PIX_FMT_NV12;
        av_opt_set(ctx_->priv_data, "preset", "p1", 0);
        av_opt_set(ctx_->priv_data, "tune", "ull", 0);
        av_opt_set(ctx_->priv_data, "zerolatency", "1", 0);
        av_opt_set(ctx_->priv_data, "delay", "0", 0);
        av_opt_set(ctx_->priv_data, "rc", "cbr", 0);
        av_opt_set(ctx_->priv_data, "intra-refresh", "1", 0);
        av_opt_set_int(ctx_->priv_data, "slices", 4, 0);
    } else if (std::string(name) == "libx264") {
        ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
        av_opt_set(ctx_->priv_data, "preset", "ultrafast", 0);
        av_opt_set(ctx_->priv_data, "tune", "zerolatency", 0);
        av_opt_set(ctx_->priv_data, "intra-refresh", "1", 0);
        av_opt_set(ctx_->priv_data, "slices", "4", 0);
    } else {
        ctx_->pix_fmt = AV_PIX_FMT_NV12;
    }

    if (avcodec_open2(ctx_, c_, nullptr) < 0) {
        avcodec_free_context(&ctx_);
        ctx_ = nullptr;
        return false;
    }

    if (!frm_) {
        frm_ = av_frame_alloc();
        frm_->format = ctx_->pix_fmt;
        frm_->width = w_;
        frm_->height = h_;
        av_frame_get_buffer(frm_, 32);
    }

    if (!pkt_) {
        pkt_ = av_packet_alloc();
    }

    return true;
}

void Enc::toNv12(const uint8_t* bgra, int stride) {
    int nth = 4;
    int slc = (h_ / 2 / nth) * 2;
    std::vector<std::thread> ths;

    uint8_t* y_p = frm_->data[0];
    int y_s = frm_->linesize[0];
    uint8_t* uv_p = frm_->data[1];
    int uv_s = frm_->linesize[1];

    for (int t = 0; t < nth; ++t) {
        int y0 = t * slc;
        int y1 = (t == nth - 1) ? h_ : (t + 1) * slc;

        ths.emplace_back([this, bgra, stride, y_p, y_s, uv_p, uv_s, y0, y1]() {
            for (int y = y0; y < y1; y += 2) {
                const uint32_t* r0 = (const uint32_t*)(bgra + y * stride);
                const uint32_t* r1 = (const uint32_t*)(bgra + (y + 1) * stride);
                uint8_t* dy0 = y_p + y * y_s;
                uint8_t* dy1 = y_p + (y + 1) * y_s;
                uint8_t* duv = uv_p + (y / 2) * uv_s;

                for (int x = 0; x < w_; x += 2) {
                    uint32_t p0 = r0[x], p1 = r0[x + 1], p2 = r1[x], p3 = r1[x + 1];

                    uint32_t b0 = (p0) & 0xFF, g0 = (p0 >> 8) & 0xFF, r_0 = (p0 >> 16) & 0xFF;
                    uint32_t b1 = (p1) & 0xFF, g1 = (p1 >> 8) & 0xFF, r_1 = (p1 >> 16) & 0xFF;
                    uint32_t b2 = (p2) & 0xFF, g2 = (p2 >> 8) & 0xFF, r_2 = (p2 >> 16) & 0xFF;
                    uint32_t b3 = (p3) & 0xFF, g3 = (p3 >> 8) & 0xFF, r_3 = (p3 >> 16) & 0xFF;

                    dy0[x]     = (uint8_t)(((66 * r_0 + 129 * g0 + 25 * b0 + 128) >> 8) + 16);
                    dy0[x + 1] = (uint8_t)(((66 * r_1 + 129 * g1 + 25 * b1 + 128) >> 8) + 16);
                    dy1[x]     = (uint8_t)(((66 * r_2 + 129 * g2 + 25 * b2 + 128) >> 8) + 16);
                    dy1[x + 1] = (uint8_t)(((66 * r_3 + 129 * g3 + 25 * b3 + 128) >> 8) + 16);

                    uint32_t ra = (r_0 + r_1 + r_2 + r_3) >> 2;
                    uint32_t ga = (g0 + g1 + g2 + g3) >> 2;
                    uint32_t ba = (b0 + b1 + b2 + b3) >> 2;

                    duv[x]     = (uint8_t)(((-38 * (int)ra - 74 * (int)ga + 112 * (int)ba + 128) >> 8) + 128);
                    duv[x + 1] = (uint8_t)(((112 * (int)ra - 94 * (int)ga - 18 * (int)ba + 128) >> 8) + 128);
                }
            }
        });
    }

    for (auto& th : ths) th.join();
}

void Enc::toYuv(const uint8_t* bgra, int stride) {
    if (!sws_) {
        sws_ = sws_getContext(w_, h_, AV_PIX_FMT_BGRA,
                              w_, h_, AV_PIX_FMT_YUV420P,
                              SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    }
    const uint8_t* s_slc[1] = { bgra };
    int s_strd[1] = { stride };
    sws_scale(sws_, s_slc, s_strd, 0, h_, frm_->data, frm_->linesize);
}

void Enc::feed(const uint8_t* bgra, int stride, uint64_t ts) {
    if (!ctx_ || !bgra) return;

    if (ctx_->pix_fmt == AV_PIX_FMT_NV12) {
        toNv12(bgra, stride);
    } else {
        toYuv(bgra, stride);
    }

    frm_->pts = seq_++;

    if (idr_.exchange(false)) {
        frm_->pict_type = AV_PICTURE_TYPE_I;
        frm_->key_frame = 1;
    } else {
        frm_->pict_type = AV_PICTURE_TYPE_NONE;
        frm_->key_frame = 0;
    }

    if (avcodec_send_frame(ctx_, frm_) < 0) return;

    uint16_t sidx = 0;
    while (avcodec_receive_packet(ctx_, pkt_) == 0) {
        HdrVid h;
        h.t = M_VID;
        h.flg = 0;
        if (pkt_->flags & AV_PKT_FLAG_KEY) h.flg |= F_KEY;
        h.slc = sidx++;
        h.seq = (uint32_t)frm_->pts;
        h.ts = ts;
        h.sz = pkt_->size;

        if (cb_) cb_(h, pkt_->data, pkt_->size);
        av_packet_unref(pkt_);
    }
}

void Enc::reqIdr() {
    idr_ = true;
}

void Enc::setBr(int br) {
    br_ = br;
    if (ctx_) {
        ctx_->bit_rate = (int64_t)br * 1000;
        ctx_->rc_max_rate = ctx_->bit_rate;
    }
}

}

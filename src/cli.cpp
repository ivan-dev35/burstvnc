#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#include <SDL2/SDL.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include "proto.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    std::string port = "8080";
    if (argc > 1) host = argv[1];
    if (argc > 2) port = argv[2];

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) return 1;

    int w = 1920, h = 1080;
    SDL_Window* win = SDL_CreateWindow("BurstVNC Client",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!win) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* rnd = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!rnd) rnd = SDL_CreateRenderer(win, -1, 0);

    SDL_Texture* tex = SDL_CreateTexture(rnd, SDL_PIXELFORMAT_IYUV,
                                         SDL_TEXTUREACCESS_STREAMING, w, h);

    const AVCodec* dec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!dec) return 1;

    AVCodecContext* ctx = avcodec_alloc_context3(dec);
    ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    ctx->flags2 |= AV_CODEC_FLAG2_FAST;
    ctx->thread_count = 2;
    avcodec_open2(ctx, dec, nullptr);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frm = av_frame_alloc();

    std::atomic<bool> run{true};
    SDL_Cursor* cur = nullptr;

    net::io_context ioc;
    tcp::resolver res(ioc);
    websocket::stream<beast::tcp_stream> ws(ioc);

    try {
        auto const ep = res.resolve(host, port);
        beast::get_lowest_layer(ws).connect(ep);
        beast::get_lowest_layer(ws).socket().set_option(tcp::no_delay(true));
        ws.binary(true);
        ws.handshake(host + ":" + port, "/ws");

        bst::MsgCtl req;
        req.c = 1;
        ws.write(net::buffer(&req, sizeof(req)));
    } catch (...) {
        return 1;
    }

    std::thread nth([&]() {
        beast::flat_buffer b;
        while (run) {
            try {
                ws.read(b);
                if (b.size() == 0) continue;

                const uint8_t* p = static_cast<const uint8_t*>(b.data().data());
                size_t sz = b.size();
                uint8_t t = p[0];

                if (t == bst::M_VID && sz >= sizeof(bst::HdrVid)) {
                    const auto* vh = reinterpret_cast<const bst::HdrVid*>(p);
                    pkt->data = (uint8_t*)(p + sizeof(bst::HdrVid));
                    pkt->size = vh->sz;
                    if (avcodec_send_packet(ctx, pkt) == 0) {
                        while (avcodec_receive_frame(ctx, frm) == 0) {
                            SDL_UpdateYUVTexture(tex, nullptr,
                                                 frm->data[0], frm->linesize[0],
                                                 frm->data[1], frm->linesize[1],
                                                 frm->data[2], frm->linesize[2]);
                        }
                    }
                } else if (t == bst::M_CUR && sz >= sizeof(bst::HdrCur)) {
                    const auto* ch = reinterpret_cast<const bst::HdrCur*>(p);
                    if (ch->w > 0 && ch->h > 0) {
                        const uint8_t* rgba = p + sizeof(bst::HdrCur);
                        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(
                            (void*)rgba, ch->w, ch->h, 32, ch->w * 4, SDL_PIXELFORMAT_RGBA32);
                        if (s) {
                            SDL_Cursor* nc = SDL_CreateColorCursor(s, ch->hx, ch->hy);
                            if (nc) {
                                SDL_SetCursor(nc);
                                if (cur) SDL_FreeCursor(cur);
                                cur = nc;
                            }
                            SDL_FreeSurface(s);
                        }
                    }
                }
                b.consume(sz);
            } catch (...) {
                break;
            }
        }
    });

    bool relM = false;
    while (run) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                run = false;
            } else if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_F11) {
                    Uint32 f = SDL_GetWindowFlags(win);
                    SDL_SetWindowFullscreen(win, (f & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                } else if (ev.key.keysym.sym == SDLK_F12) {
                    relM = !relM;
                    SDL_SetRelativeMouseMode(relM ? SDL_TRUE : SDL_FALSE);
                } else {
                    bst::MsgKey km;
                    km.sym = ev.key.keysym.sym;
                    km.d = 1;
                    ws.write(net::buffer(&km, sizeof(km)));
                }
            } else if (ev.type == SDL_KEYUP) {
                bst::MsgKey km;
                km.sym = ev.key.keysym.sym;
                km.d = 0;
                ws.write(net::buffer(&km, sizeof(km)));
            } else if (ev.type == SDL_MOUSEMOTION) {
                if (relM) {
                    bst::MsgMRel rm;
                    rm.dx = ev.motion.xrel;
                    rm.dy = ev.motion.yrel;
                    ws.write(net::buffer(&rm, sizeof(rm)));
                } else {
                    int ww, wh;
                    SDL_GetWindowSize(win, &ww, &wh);
                    bst::MsgMAbs am;
                    am.x = (uint16_t)((ev.motion.x * w) / ww);
                    am.y = (uint16_t)((ev.motion.y * h) / wh);
                    ws.write(net::buffer(&am, sizeof(am)));
                }
            } else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                bst::MsgMBtn bm;
                bm.b = ev.button.button;
                bm.d = (ev.type == SDL_MOUSEBUTTONDOWN) ? 1 : 0;
                ws.write(net::buffer(&bm, sizeof(bm)));
            } else if (ev.type == SDL_MOUSEWHEEL) {
                bst::MsgMWhl wm;
                wm.dy = ev.wheel.y;
                wm.dx = ev.wheel.x;
                ws.write(net::buffer(&wm, sizeof(wm)));
            }
        }

        SDL_RenderClear(rnd);
        SDL_RenderCopy(rnd, tex, nullptr, nullptr);
        SDL_RenderPresent(rnd);
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }

    try { ws.close(websocket::close_code::normal); } catch (...) {}
    if (nth.joinable()) nth.join();
    if (cur) SDL_FreeCursor(cur);
    av_frame_free(&frm);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(rnd);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

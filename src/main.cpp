#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <string>

#include "proto.hpp"
#include "cap.hpp"
#include "cur.hpp"
#include "enc.hpp"
#include "inp.hpp"
#include "aud.hpp"
#include "srv.hpp"

static std::atomic<bool> g_run{true};

static void onSig(int) {
    g_run = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, onSig);
    std::signal(SIGTERM, onSig);

    std::string dev = "auto";
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    int fps = 60;
    int br = 8000;
    std::string pref = "auto";
    std::string root = "/kaggle/working/burstvnc/web";
    bool audOn = true;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--display" && i + 1 < argc) dev = argv[++i];
        else if (a == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (a == "--fps" && i + 1 < argc) fps = std::stoi(argv[++i]);
        else if (a == "--bitrate" && i + 1 < argc) br = std::stoi(argv[++i]);
        else if (a == "--encoder" && i + 1 < argc) pref = argv[++i];
        else if (a == "--web-root" && i + 1 < argc) root = argv[++i];
        else if (a == "--no-audio") audOn = false;
    }

    auto cap = std::make_unique<bst::Cap>(dev);
    if (!cap->init()) return 1;

    int w = cap->getW();
    int h = cap->getH();

    auto srv = std::make_shared<bst::Srv>(host, port, root);
    if (!srv->start()) return 1;

    auto cur = std::make_unique<bst::Cur>(cap->getDpy(), cap->getRoot());
    if (cur->init()) {
        cur->hide();
        cur->setCb([srv](const bst::HdrCur& ch, const std::vector<uint8_t>& cd) {
            srv->bcastC(ch, cd);
        });
    }

    auto enc = std::make_unique<bst::Enc>(w, h, fps, br, pref);
    if (!enc->init()) return 1;

    enc->setCb([srv](const bst::HdrVid& vh, const uint8_t* vd, size_t vsz) {
        srv->bcastV(vh, vd, vsz);
    });

    auto inp = std::make_unique<bst::Inp>(cap->getDpy(), cap->getRoot(), w, h);
    inp->init();

    srv->setConnHdlr([&](uint64_t sid) {
        enc->reqIdr();

        bst::HdrCfg cfg;
        cfg.t = bst::M_CFG;
        cfg.w = w;
        cfg.h = h;
        cfg.fps = fps;
        std::strncpy(cfg.cdc, enc->getCdc().c_str(), sizeof(cfg.cdc) - 1);
        auto mcfg = std::make_shared<std::vector<uint8_t>>(sizeof(bst::HdrCfg));
        std::memcpy(mcfg->data(), &cfg, sizeof(bst::HdrCfg));
        srv->sendTo(sid, mcfg);

        bst::HdrCur ch;
        std::vector<uint8_t> cd;
        if (cur->get(ch, cd)) {
            auto mcur = std::make_shared<std::vector<uint8_t>>(sizeof(bst::HdrCur) + cd.size());
            std::memcpy(mcur->data(), &ch, sizeof(bst::HdrCur));
            std::memcpy(mcur->data() + sizeof(bst::HdrCur), cd.data(), cd.size());
            srv->sendTo(sid, mcur);
        }
    });

    srv->setInpHdlr([&](const uint8_t* d, size_t sz) {
        if (sz == 0) return;
        uint8_t t = d[0];
        if (t == bst::M_M_ABS && sz >= sizeof(bst::MsgMAbs)) {
            inp->mAbs(*reinterpret_cast<const bst::MsgMAbs*>(d));
        } else if (t == bst::M_M_REL && sz >= sizeof(bst::MsgMRel)) {
            inp->mRel(*reinterpret_cast<const bst::MsgMRel*>(d));
        } else if (t == bst::M_M_BTN && sz >= sizeof(bst::MsgMBtn)) {
            inp->mBtn(*reinterpret_cast<const bst::MsgMBtn*>(d));
        } else if (t == bst::M_M_WHL && sz >= sizeof(bst::MsgMWhl)) {
            inp->mWhl(*reinterpret_cast<const bst::MsgMWhl*>(d));
        } else if (t == bst::M_KEY && sz >= sizeof(bst::MsgKey)) {
            inp->key(*reinterpret_cast<const bst::MsgKey*>(d));
        } else if (t == bst::M_CTL && sz >= sizeof(bst::MsgCtl)) {
            const auto* c = reinterpret_cast<const bst::MsgCtl*>(d);
            if (c->c == 1) enc->reqIdr();
            else if (c->c == 2) enc->setBr(c->p);
        }
    });

    std::unique_ptr<bst::Aud> aud;
    if (audOn) {
        aud = std::make_unique<bst::Aud>(48000, 2);
        if (aud->init()) {
            aud->setCb([srv](const bst::HdrAud& ah, const uint8_t* ad, size_t asz) {
                srv->bcastA(ah, ad, asz);
            });
            aud->start();
        }
    }

    std::cout << "[BurstVNC] Running: " << w << "x" << h << "@" << fps << "fps [" << enc->getCdc() << "] on port " << port << std::endl;

    const auto dt = std::chrono::microseconds(1000000 / fps);
    auto nxt = std::chrono::steady_clock::now();

    while (g_run) {
        auto now = std::chrono::steady_clock::now();
        if (now < nxt) {
            std::this_thread::sleep_until(nxt);
        }
        nxt += dt;

        cur->chk();

        if (cap->grab()) {
            auto ts = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            enc->feed(cap->getData(), cap->getS(), ts);
        }
    }

    if (aud) aud->stop();
    srv->stop();
    return 0;
}

#pragma once
#include <vector>
#include <cstdint>
#include <functional>
#include <mutex>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include "proto.hpp"

namespace bst {

class Cur {
public:
    using Cb = std::function<void(const HdrCur& h, const std::vector<uint8_t>& d)>;

    Cur(Display* dpy, Window root);
    ~Cur();

    bool init();
    void setCb(Cb cb) { cb_ = cb; }
    void chk();
    void hide();
    bool get(HdrCur& h, std::vector<uint8_t>& d);

private:
    Display* dpy_ = nullptr;
    Window root_ = 0;
    int ev_ = 0;
    int er_ = 0;
    Cb cb_;

    std::mutex mtx_;
    uint64_t hsh_ = 0;
    HdrCur hdr_;
    std::vector<uint8_t> rgba_;
    Cursor inv_ = 0;
};

}

#include "cur.hpp"
#include <cstring>

namespace bst {

Cur::Cur(Display* dpy, Window root) : dpy_(dpy), root_(root) {}

Cur::~Cur() {
    if (inv_ && dpy_) {
        XFreeCursor(dpy_, inv_);
    }
}

bool Cur::init() {
    if (!dpy_) return false;
    if (!XFixesQueryExtension(dpy_, &ev_, &er_)) return false;

    XFixesSelectCursorInput(dpy_, root_, XFixesDisplayCursorNotifyMask);
    XSync(dpy_, False);
    chk();
    return true;
}

void Cur::hide() {
    if (!dpy_ || inv_) return;

    Pixmap pm = XCreateBitmapFromData(dpy_, root_, "\0", 1, 1);
    XColor col;
    col.pixel = 0;
    col.red = col.green = col.blue = 0;
    col.flags = DoRed | DoGreen | DoBlue;

    inv_ = XCreatePixmapCursor(dpy_, pm, pm, &col, &col, 0, 0);
    XFreePixmap(dpy_, pm);

    XDefineCursor(dpy_, root_, inv_);
    XSync(dpy_, False);
}

bool Cur::get(HdrCur& h, std::vector<uint8_t>& d) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (rgba_.empty()) return false;
    h = hdr_;
    d = rgba_;
    return true;
}

void Cur::chk() {
    if (!dpy_) return;
    XFixesCursorImage* ci = XFixesGetCursorImage(dpy_);
    if (!ci) return;

    uint64_t h = 14695981039346656037ULL;
    auto hf = [&](const void* p, size_t sz) {
        const uint8_t* u = (const uint8_t*)p;
        for (size_t i = 0; i < sz; ++i) {
            h ^= u[i];
            h *= 1099511628211ULL;
        }
    };

    hf(&ci->width, sizeof(ci->width));
    hf(&ci->height, sizeof(ci->height));
    hf(&ci->xhot, sizeof(ci->xhot));
    hf(&ci->yhot, sizeof(ci->yhot));

    size_t cnt = (size_t)ci->width * ci->height;
    hf(ci->pixels, cnt * sizeof(unsigned long));

    if (h != hsh_) {
        hsh_ = h;

        HdrCur nh;
        nh.t = M_CUR;
        nh.w = ci->width;
        nh.h = ci->height;
        nh.hx = ci->xhot;
        nh.hy = ci->yhot;
        nh.sz = ci->width * ci->height * 4;

        std::vector<uint8_t> nr(nh.sz);
        for (size_t i = 0; i < cnt; ++i) {
            unsigned long p = ci->pixels[i];
            uint8_t a = (p >> 24) & 0xFF;
            uint8_t r = (p >> 16) & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = (p) & 0xFF;

            nr[i * 4 + 0] = r;
            nr[i * 4 + 1] = g;
            nr[i * 4 + 2] = b;
            nr[i * 4 + 3] = a;
        }

        {
            std::lock_guard<std::mutex> lk(mtx_);
            hdr_ = nh;
            rgba_ = nr;
        }

        if (cb_) cb_(nh, nr);
    }

    XFree(ci);
}

}

#include "inp.hpp"
#include <algorithm>
#include <X11/keysym.h>

namespace bst {

Inp::Inp(Display* dpy, Window root, int w, int h)
    : dpy_(dpy), root_(root), w_(w), h_(h) {}

Inp::~Inp() {}

bool Inp::init() {
    if (!dpy_) return false;
    int eb, rb, maj, min;
    if (!XTestQueryExtension(dpy_, &eb, &rb, &maj, &min)) return false;
    ok_ = true;
    return true;
}

void Inp::mAbs(const MsgMAbs& m) {
    if (!ok_) return;
    int x = std::clamp((int)m.x, 0, w_ - 1);
    int y = std::clamp((int)m.y, 0, h_ - 1);
    XTestFakeMotionEvent(dpy_, -1, x, y, CurrentTime);
    XFlush(dpy_);
}

void Inp::mRel(const MsgMRel& m) {
    if (!ok_) return;
    Window rr, cr;
    int rx, ry, wx, wy;
    unsigned int mr;
    if (XQueryPointer(dpy_, root_, &rr, &cr, &rx, &ry, &wx, &wy, &mr)) {
        int tx = std::clamp(rx + m.dx, 0, w_ - 1);
        int ty = std::clamp(ry + m.dy, 0, h_ - 1);
        XTestFakeMotionEvent(dpy_, -1, tx, ty, CurrentTime);
        XFlush(dpy_);
    }
}

void Inp::mBtn(const MsgMBtn& m) {
    if (!ok_) return;
    XTestFakeButtonEvent(dpy_, m.b, m.d ? True : False, CurrentTime);
    XFlush(dpy_);
}

void Inp::mWhl(const MsgMWhl& m) {
    if (!ok_) return;
    if (m.dy != 0) {
        unsigned int b = (m.dy > 0) ? 4 : 5;
        int n = std::abs(m.dy);
        for (int i = 0; i < n; ++i) {
            XTestFakeButtonEvent(dpy_, b, True, CurrentTime);
            XTestFakeButtonEvent(dpy_, b, False, CurrentTime);
        }
    }
    if (m.dx != 0) {
        unsigned int b = (m.dx > 0) ? 7 : 6;
        int n = std::abs(m.dx);
        for (int i = 0; i < n; ++i) {
            XTestFakeButtonEvent(dpy_, b, True, CurrentTime);
            XTestFakeButtonEvent(dpy_, b, False, CurrentTime);
        }
    }
    XFlush(dpy_);
}

void Inp::key(const MsgKey& m) {
    if (!ok_) return;
    KeyCode kc = XKeysymToKeycode(dpy_, m.sym);
    if (kc == 0) return;
    XTestFakeKeyEvent(dpy_, kc, m.d ? True : False, CurrentTime);
    XFlush(dpy_);
}

}

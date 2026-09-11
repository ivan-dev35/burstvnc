#pragma once
#include <cstdint>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include "proto.hpp"

namespace bst {

class Inp {
public:
    Inp(Display* dpy, Window root, int w, int h);
    ~Inp();

    bool init();
    void mAbs(const MsgMAbs& m);
    void mRel(const MsgMRel& m);
    void mBtn(const MsgMBtn& m);
    void mWhl(const MsgMWhl& m);
    void key(const MsgKey& m);

private:
    Display* dpy_ = nullptr;
    Window root_ = 0;
    int w_ = 0;
    int h_ = 0;
    bool ok_ = false;
};

}

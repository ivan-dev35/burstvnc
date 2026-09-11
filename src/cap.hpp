#pragma once
#include <string>
#include <cstdint>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

namespace bst {

class Cap {
public:
    Cap(const std::string& dev);
    ~Cap();

    bool init();
    bool grab();

    int getW() const { return w_; }
    int getH() const { return h_; }
    int getS() const { return img_ ? img_->bytes_per_line : 0; }
    const uint8_t* getData() const { return img_ ? (const uint8_t*)img_->data : nullptr; }
    Display* getDpy() { return dpy_; }
    Window getRoot() { return root_; }

private:
    std::string dev_;
    Display* dpy_ = nullptr;
    int scr_ = 0;
    Window root_ = 0;
    int w_ = 0;
    int h_ = 0;
    XShmSegmentInfo shm_;
    XImage* img_ = nullptr;
    bool att_ = false;
};

}

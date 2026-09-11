#include "cap.hpp"
#include <iostream>
#include <cstring>

namespace bst {

Cap::Cap(const std::string& dev) : dev_(dev) {
    std::memset(&shm_, 0, sizeof(shm_));
}

Cap::~Cap() {
    if (dpy_) {
        if (att_) {
            XShmDetach(dpy_, &shm_);
            att_ = false;
        }
        if (img_) {
            XDestroyImage(img_);
            img_ = nullptr;
        }
        if (shm_.shmaddr) {
            shmdt(shm_.shmaddr);
            shm_.shmaddr = nullptr;
        }
        if (shm_.shmid > 0) {
            shmctl(shm_.shmid, IPC_RMID, nullptr);
            shm_.shmid = 0;
        }
        XCloseDisplay(dpy_);
        dpy_ = nullptr;
    }
}

bool Cap::init() {
    dpy_ = XOpenDisplay(dev_.empty() ? nullptr : dev_.c_str());
    if (!dpy_) return false;

    scr_ = DefaultScreen(dpy_);
    root_ = RootWindow(dpy_, scr_);
    w_ = DisplayWidth(dpy_, scr_);
    h_ = DisplayHeight(dpy_, scr_);

    int maj, min;
    Bool px;
    if (!XShmQueryVersion(dpy_, &maj, &min, &px)) return false;

    img_ = XShmCreateImage(dpy_, DefaultVisual(dpy_, scr_),
                          DefaultDepth(dpy_, scr_), ZPixmap,
                          nullptr, &shm_, w_, h_);
    if (!img_) return false;

    shm_.shmid = shmget(IPC_PRIVATE, img_->bytes_per_line * img_->height, IPC_CREAT | 0777);
    if (shm_.shmid < 0) return false;

    shm_.shmaddr = img_->data = (char*)shmat(shm_.shmid, 0, 0);
    if (shm_.shmaddr == (char*)-1) return false;

    shm_.readOnly = False;
    if (!XShmAttach(dpy_, &shm_)) return false;
    att_ = true;
    XSync(dpy_, False);
    return true;
}

bool Cap::grab() {
    if (!dpy_ || !img_) return false;
    return XShmGetImage(dpy_, root_, img_, 0, 0, AllPlanes) != 0;
}

}

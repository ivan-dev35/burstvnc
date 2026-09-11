#include "cap.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>

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
    if (dev_.empty() || dev_ == "auto") {
        const char* envD = getenv("DISPLAY");
        if (envD && envD[0]) {
            dpy_ = XOpenDisplay(envD);
            if (dpy_) dev_ = envD;
        }
        if (!dpy_) {
            const char* cands[] = {":0", ":1", ":99", nullptr};
            for (int i = 0; cands[i]; ++i) {
                dpy_ = XOpenDisplay(cands[i]);
                if (dpy_) {
                    dev_ = cands[i];
                    break;
                }
            }
        }
        if (!dpy_) {
            dev_ = ":99";
            std::string cmd = "Xvfb " + dev_ + " -screen 0 1920x1080x24 -ac +extension COMPOSITE +extension DAMAGE +extension RANDR +extension GLX >/dev/null 2>&1 &";
            int r = system(cmd.c_str());
            (void)r;
            for (int i = 0; i < 30; ++i) {
                usleep(50000);
                dpy_ = XOpenDisplay(dev_.c_str());
                if (dpy_) break;
            }
            if (dpy_) {
                std::string wmCmd = "DISPLAY=" + dev_ + " (which xfce4-session >/dev/null && startxfce4 || which openbox >/dev/null && openbox || xterm) >/dev/null 2>&1 &";
                r = system(wmCmd.c_str());
                (void)r;
            }
        }
    } else {
        dpy_ = XOpenDisplay(dev_.c_str());
        if (!dpy_) {
            std::string cmd = "Xvfb " + dev_ + " -screen 0 1920x1080x24 -ac +extension COMPOSITE +extension DAMAGE +extension RANDR +extension GLX >/dev/null 2>&1 &";
            int r = system(cmd.c_str());
            (void)r;
            for (int i = 0; i < 30; ++i) {
                usleep(50000);
                dpy_ = XOpenDisplay(dev_.c_str());
                if (dpy_) break;
            }
        }
    }

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

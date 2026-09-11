#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace bst {

enum MsgT : uint8_t {
    M_VID = 0x01,
    M_AUD = 0x02,
    M_CUR = 0x03,
    M_PNG = 0x04,
    M_CFG = 0x05,

    M_M_ABS = 0x10,
    M_M_REL = 0x11,
    M_M_BTN = 0x12,
    M_M_WHL = 0x13,
    M_KEY   = 0x14,
    M_PNG_Q = 0x15,
    M_CTL   = 0x16,
};

enum VidFlg : uint8_t {
    F_KEY = 0x01,
    F_INTRA = 0x02,
    F_SLICE0 = 0x04,
    F_SLICE1 = 0x08,
};

#pragma pack(push, 1)

struct HdrVid {
    uint8_t  t = M_VID;
    uint8_t  flg = 0;
    uint16_t slc = 0;
    uint32_t seq = 0;
    uint64_t ts = 0;
    uint32_t sz = 0;
};

struct HdrAud {
    uint8_t  t = M_AUD;
    uint16_t sz = 0;
    uint64_t ts = 0;
};

struct HdrCur {
    uint8_t  t = M_CUR;
    uint16_t w = 0;
    uint16_t h = 0;
    int16_t  hx = 0;
    int16_t  hy = 0;
    uint32_t sz = 0;
};

struct HdrPng {
    uint8_t  t = M_PNG;
    uint64_t c_ts = 0;
    uint64_t s_ts = 0;
};

struct HdrCfg {
    uint8_t  t = M_CFG;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t fps = 0;
    char     cdc[16] = {0};
};

struct MsgMAbs {
    uint8_t  t = M_M_ABS;
    uint16_t x = 0;
    uint16_t y = 0;
};

struct MsgMRel {
    uint8_t t = M_M_REL;
    int16_t dx = 0;
    int16_t dy = 0;
};

struct MsgMBtn {
    uint8_t t = M_M_BTN;
    uint8_t b = 0;
    uint8_t d = 0;
};

struct MsgMWhl {
    uint8_t t = M_M_WHL;
    int16_t dy = 0;
    int16_t dx = 0;
};

struct MsgKey {
    uint8_t  t = M_KEY;
    uint32_t sym = 0;
    uint8_t  d = 0;
};

struct MsgPng {
    uint8_t  t = M_PNG_Q;
    uint64_t ts = 0;
};

struct MsgCtl {
    uint8_t  t = M_CTL;
    uint8_t  c = 0;
    uint32_t p = 0;
};

#pragma pack(pop)

}

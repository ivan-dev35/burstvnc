# BurstVNC

Next-generation ultra-low-latency remote desktop system with decoupled hardware cursor and multi-tier NVENC / CPU streaming.

## Features
- **Zero-Latency Hardware Decoupled Cursor:** Cursor rendered client-side at native refresh rates (120/144/240Hz) with 0ms perceived lag.
- **Speculative Tactile Ripple Feedback:** Instant click and key visual reaction.
- **Multi-Backend:** Turing/Ampere NVENC, VAAPI, and multi-core CPU libx264 with AVX2 SIMD.
- **3D Gaming Pointer Lock:** Raw relative delta motion for FPS and 3D games.
- **Periodic Intra-Refresh (PIR):** Flat bandwidth stream, self-healing without heavy IDR bursts.
- **Dual Client:** Hardware WebCodecs GPU browser client and native SDL2 C++ client.

## Build
```bash
cd /kaggle/working/burstvnc/build
cmake .. -GNinja
ninja
```

## Quick Start
```bash
/kaggle/working/burstvnc/scripts/run.sh
```

## Manual Start
```bash
./build/burst_srv --display :99 --port 8080 --fps 60 --bitrate 8000
./build/burst_srv --display :99 --port 8080 --fps 120 --bitrate 12000 --encoder nvenc
./build/burst_srv --display :99 --port 8080 --fps 60 --bitrate 6000 --encoder x264
```

## Connect
- Browser: `http://<HOST>:8080/`
- Native: `./build/burst_cli <HOST> 8080`

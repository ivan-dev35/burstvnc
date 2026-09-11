import asyncio
import struct
import time
import websockets
import urllib.request

def test_http():
    req = urllib.request.urlopen("http://127.0.0.1:8080/")
    assert req.status == 200
    html = req.read().decode('utf-8')
    assert "BurstVNC" in html

async def test_ws():
    uri = "ws://127.0.0.1:8080/ws"
    async with websockets.connect(uri) as ws:
        # Request IDR
        req = struct.pack("<BBIB", 0x16, 1, 0, 0)[:6]
        await ws.send(req)

        # Send Ping
        t0 = time.time()
        pmsg = struct.pack("<BQ", 0x15, int(t0 * 1_000_000))
        await ws.send(pmsg)

        # Input Tests
        await ws.send(struct.pack("<BHH", 0x10, 400, 300))
        await ws.send(struct.pack("<Bhh", 0x11, 10, -5))
        await ws.send(struct.pack("<BBB", 0x12, 1, 1))
        await ws.send(struct.pack("<BBB", 0x12, 1, 0))
        await ws.send(struct.pack("<Bhh", 0x13, 1, 0))
        await ws.send(struct.pack("<BIBB", 0x14, 0x61, 1, 0)[:6])
        await ws.send(struct.pack("<BIBB", 0x14, 0x61, 0, 0)[:6])

        f_cnt = 0
        cur_cnt = 0
        cfg_cnt = 0
        pong_ok = False

        st = time.time()
        while time.time() - st < 2.0:
            try:
                m = await asyncio.wait_for(ws.recv(), timeout=1.0)
                if not isinstance(m, bytes) or len(m) == 0:
                    continue
                t = m[0]
                if t == 0x01:
                    f_cnt += 1
                elif t == 0x03:
                    cur_cnt += 1
                elif t == 0x04:
                    pong_ok = True
                elif t == 0x05:
                    cfg_cnt += 1
            except asyncio.TimeoutError:
                break

        dur = time.time() - st
        fps = f_cnt / dur
        print(f"Frames: {f_cnt} ({fps:.1f} FPS), Configs: {cfg_cnt}, Cursors: {cur_cnt}, Pong: {pong_ok}")
        assert f_cnt > 30
        assert pong_ok

if __name__ == '__main__':
    test_http()
    asyncio.run(test_ws())
    print("ALL TESTS PASSED")

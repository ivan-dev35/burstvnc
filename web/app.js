class Bst {
    constructor() {
        this.c = document.getElementById('cvs');
        this.ctx = this.c.getContext('2d', { alpha: false, desynchronized: true });
        this.fx = document.getElementById('fx');
        this.fxCtx = this.fx.getContext('2d');

        this.vFps = document.getElementById('vFps');
        this.vRtt = document.getElementById('vRtt');
        this.vDec = document.getElementById('vDec');
        this.vBps = document.getElementById('vBps');
        this.vCdc = document.getElementById('vCdc');
        this.vMod = document.getElementById('vMod');

        this.ws = null;
        this.dec = null;
        this.rdy = false;
        this.fKey = false;

        this.w = 1920;
        this.h = 1080;
        this.gm = false;

        this.fpsC = 0;
        this.byteC = 0;
        this.lastT = performance.now();
        this.rtt = 0;
        this.decT = 0;
        this.rips = [];
        this.cUrl = null;

        this.c.width = this.w;
        this.c.height = this.h;
        this.resz();
        window.addEventListener('resize', () => this.resz());
        this.bind();
        this.fxL();
    }

    resz() {
        this.fx.width = window.innerWidth;
        this.fx.height = window.innerHeight;
    }

    conn() {
        const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
        this.ws = new WebSocket(`${proto}//${location.host}/ws`);
        this.ws.binaryType = 'arraybuffer';

        this.ws.onopen = () => {
            document.getElementById('ovl').style.display = 'none';
            this.initDec();
            this.pngL();
        };

        this.ws.onmessage = (e) => this.onMsg(e.data);
        this.ws.onclose = () => {
            document.getElementById('ovl').style.display = 'flex';
        };
    }

    initDec() {
        if (!('VideoDecoder' in window)) return;
        this.dec = new VideoDecoder({
            output: (f) => this.onFrm(f),
            error: () => this.reqIdr()
        });

        const cfg = {
            codec: 'avc1.42002a',
            optimizeForLatency: true,
            hardwareAcceleration: 'prefer-hardware'
        };

        VideoDecoder.isConfigSupported(cfg).then((r) => {
            this.dec.configure(cfg);
            this.rdy = true;
            this.reqIdr();
        });
    }

    onFrm(f) {
        const t0 = performance.now();
        this.ctx.drawImage(f, 0, 0, this.c.width, this.c.height);
        this.decT = (performance.now() - t0).toFixed(1);
        f.close();
        this.fpsC++;
    }

    onMsg(buf) {
        if (!buf || buf.byteLength === 0) return;
        this.byteC += buf.byteLength;
        const v = new DataView(buf);
        const t = v.getUint8(0);

        if (t === 0x01) {
            const flg = v.getUint8(1);
            const ts = Number(v.getBigUint64(8, true));
            const sz = v.getUint32(16, true);
            const isK = (flg & 0x01) !== 0;

            if (!this.fKey) {
                if (!isK) return;
                this.fKey = true;
            }

            if (this.rdy && this.dec.state === 'configured') {
                const chunk = new Uint8Array(buf, 20, sz);
                try {
                    this.dec.decode(new EncodedVideoChunk({
                        type: isK ? 'key' : 'delta',
                        timestamp: ts,
                        data: chunk
                    }));
                } catch (_) {}
            }
        } else if (t === 0x03) {
            this.onCur(v, buf);
        } else if (t === 0x04) {
            const cTs = Number(v.getBigUint64(1, true));
            this.rtt = Math.max(0, ((performance.now() * 1000 - cTs) / 1000)).toFixed(1);
            this.vRtt.innerText = `${this.rtt} ms`;
        } else if (t === 0x05) {
            this.w = v.getUint16(1, true);
            this.h = v.getUint16(3, true);
            this.c.width = this.w;
            this.c.height = this.h;
            let cdc = '';
            for (let i = 7; i < 23; ++i) {
                let ch = v.getUint8(i);
                if (!ch) break;
                cdc += String.fromCharCode(ch);
            }
            this.vCdc.innerText = cdc;
        }
    }

    onCur(v, buf) {
        const cw = v.getUint16(1, true);
        const ch = v.getUint16(3, true);
        const hx = v.getInt16(5, true);
        const hy = v.getInt16(7, true);
        const sz = v.getUint32(9, true);
        if (cw === 0 || ch === 0 || sz === 0) return;

        const px = new Uint8ClampedArray(buf, 13, sz);
        const oc = document.createElement('canvas');
        oc.width = cw;
        oc.height = ch;
        const octx = oc.getContext('2d');
        octx.putImageData(new ImageData(px, cw, ch), 0, 0);

        oc.toBlob((b) => {
            if (this.cUrl) URL.revokeObjectURL(this.cUrl);
            this.cUrl = URL.createObjectURL(b);
            this.c.style.cursor = `url('${this.cUrl}') ${hx} ${hy}, auto`;
        });
    }

    pngL() {
        setInterval(() => {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                const now = BigInt(Math.floor(performance.now() * 1000));
                const b = new ArrayBuffer(9);
                const v = new DataView(b);
                v.setUint8(0, 0x15);
                v.setBigUint64(1, now, true);
                this.ws.send(b);
            }

            const t = performance.now();
            const dt = (t - this.lastT) / 1000;
            if (dt >= 1.0) {
                this.vFps.innerText = `${Math.round(this.fpsC / dt)}`;
                this.vBps.innerText = `${((this.byteC * 8) / (dt * 1000000)).toFixed(2)} Mbps`;
                this.vDec.innerText = `${this.decT} ms`;
                this.fpsC = 0;
                this.byteC = 0;
                this.lastT = t;
            }
        }, 500);
    }

    reqIdr() {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            const b = new ArrayBuffer(6);
            const v = new DataView(b);
            v.setUint8(0, 0x16);
            v.setUint8(1, 1);
            v.setUint32(2, 0, true);
            this.ws.send(b);
        }
    }

    setBr(br) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            const b = new ArrayBuffer(6);
            const v = new DataView(b);
            v.setUint8(0, 0x16);
            v.setUint8(1, 2);
            v.setUint32(2, br, true);
            this.ws.send(b);
        }
    }

    bind() {
        document.getElementById('bConn').addEventListener('click', () => this.conn());
        document.getElementById('bIdr').addEventListener('click', () => this.reqIdr());
        document.getElementById('bFs').addEventListener('click', () => this.togFs());
        document.getElementById('bMod').addEventListener('click', () => this.togGm());
        document.getElementById('sBr').addEventListener('change', (e) => this.setBr(parseInt(e.target.value)));
        document.getElementById('hBtn').addEventListener('click', () => {
            const h = document.getElementById('hud');
            h.style.display = h.style.display === 'none' ? 'block' : 'none';
        });

        this.c.addEventListener('pointermove', (e) => this.onMv(e));
        this.c.addEventListener('pointerdown', (e) => this.onBtn(e, 1));
        this.c.addEventListener('pointerup', (e) => this.onBtn(e, 0));
        this.c.addEventListener('contextmenu', (e) => e.preventDefault());
        this.c.addEventListener('wheel', (e) => this.onWhl(e), { passive: false });

        window.addEventListener('keydown', (e) => this.onK(e, 1));
        window.addEventListener('keyup', (e) => this.onK(e, 0));

        document.addEventListener('pointerlockchange', () => {
            this.gm = (document.pointerLockElement === this.c);
            const b = document.getElementById('bMod');
            if (this.gm) {
                b.classList.add('act');
                this.vMod.innerText = '3D Lock';
            } else {
                b.classList.remove('act');
                this.vMod.innerText = '2D';
            }
        });
    }

    onMv(e) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
        if (this.gm) {
            const dx = e.movementX || 0;
            const dy = e.movementY || 0;
            if (dx === 0 && dy === 0) return;
            const b = new ArrayBuffer(5);
            const v = new DataView(b);
            v.setUint8(0, 0x11);
            v.setInt16(1, dx, true);
            v.setInt16(3, dy, true);
            this.ws.send(b);
        } else {
            const r = this.c.getBoundingClientRect();
            const sx = this.w / r.width;
            const sy = this.h / r.height;
            const x = Math.round(Math.max(0, Math.min(this.w - 1, (e.clientX - r.left) * sx)));
            const y = Math.round(Math.max(0, Math.min(this.h - 1, (e.clientY - r.top) * sy)));
            const b = new ArrayBuffer(5);
            const v = new DataView(b);
            v.setUint8(0, 0x10);
            v.setUint16(1, x, true);
            v.setUint16(3, y, true);
            this.ws.send(b);
        }
    }

    onBtn(e, d) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
        e.preventDefault();
        let b = 1;
        if (e.button === 1) b = 2;
        else if (e.button === 2) b = 3;

        if (d) this.addRip(e.clientX, e.clientY);

        const buf = new ArrayBuffer(3);
        const v = new DataView(buf);
        v.setUint8(0, 0x12);
        v.setUint8(1, b);
        v.setUint8(2, d);
        this.ws.send(buf);
    }

    onWhl(e) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
        e.preventDefault();
        const dy = e.deltaY > 0 ? 1 : (e.deltaY < 0 ? -1 : 0);
        const dx = e.deltaX > 0 ? 1 : (e.deltaX < 0 ? -1 : 0);
        const buf = new ArrayBuffer(5);
        const v = new DataView(buf);
        v.setUint8(0, 0x13);
        v.setInt16(1, dy, true);
        v.setInt16(3, dx, true);
        this.ws.send(buf);
    }

    onK(e, d) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
        if (['Tab', 'Alt', 'F1', 'F5', 'F12'].includes(e.key)) e.preventDefault();

        const sym = this.toSym(e);
        if (!sym) return;

        const buf = new ArrayBuffer(6);
        const v = new DataView(buf);
        v.setUint8(0, 0x14);
        v.setUint32(1, sym, true);
        v.setUint8(5, d);
        this.ws.send(buf);
    }

    toSym(e) {
        const m = {
            'Backspace': 0xff08, 'Tab': 0xff09, 'Enter': 0xff0d, 'Escape': 0xff1b,
            'Delete': 0xffff, 'Home': 0xff50, 'ArrowLeft': 0xff51, 'ArrowUp': 0xff52,
            'ArrowRight': 0xff53, 'ArrowDown': 0xff54, 'PageUp': 0xff55, 'PageDown': 0xff56,
            'End': 0xff57, 'Shift': 0xffe1, 'Control': 0xffe3, 'Alt': 0xffe9,
            'Meta': 0xffeb, 'CapsLock': 0xffe5, 'F1': 0xffbe, 'F2': 0xffbf,
            'F3': 0xffc0, 'F4': 0xffc1, 'F5': 0xffc2, 'F6': 0xffc3,
            'F7': 0xffc4, 'F8': 0xffc5, 'F9': 0xffc6, 'F10': 0xffc7,
            'F11': 0xffc8, 'F12': 0xffc9
        };
        if (m[e.key]) return m[e.key];
        if (e.key.length === 1) return e.key.charCodeAt(0);
        return 0;
    }

    addRip(x, y) {
        this.rips.push({ x, y, r: 4, mr: 24, a: .8, t: performance.now() });
    }

    fxL() {
        const loop = () => {
            this.fxCtx.clearRect(0, 0, this.fx.width, this.fx.height);
            const now = performance.now();
            for (let i = this.rips.length - 1; i >= 0; --i) {
                const rp = this.rips[i];
                const p = (now - rp.t) / 150;
                if (p >= 1) {
                    this.rips.splice(i, 1);
                    continue;
                }
                const cr = rp.r + (rp.mr - rp.r) * p;
                const ca = rp.a * (1 - p);
                this.fxCtx.beginPath();
                this.fxCtx.arc(rp.x, rp.y, cr, 0, Math.PI * 2);
                this.fxCtx.strokeStyle = `rgba(0,255,180,${ca})`;
                this.fxCtx.lineWidth = 2 * (1 - p);
                this.fxCtx.stroke();
            }
            requestAnimationFrame(loop);
        };
        requestAnimationFrame(loop);
    }

    togFs() {
        if (!document.fullscreenElement) {
            document.documentElement.requestFullscreen().catch(() => {});
        } else {
            document.exitFullscreen().catch(() => {});
        }
    }

    togGm() {
        if (!this.gm) this.c.requestPointerLock();
        else document.exitPointerLock();
    }
}

window.addEventListener('DOMContentLoaded', () => {
    window.bst = new Bst();
});

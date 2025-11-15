// ws_frag_no_ping.js
// Sends a text message split across multiple frames (no ping interleaved).

const net = require('net');
const crypto = require('crypto');

const HOST = '127.0.0.1';
const PORT = 8080;
const PATH = '/chat';

function b64(x) {
    return Buffer.from(x).toString('base64');
}

function makeHandshake() {
    const key = b64(crypto.randomBytes(16));
    return (
        `GET ${PATH} HTTP/1.1\r\n` +
        `Host: ${HOST}:${PORT}\r\n` +
        `Upgrade: websocket\r\n` +
        `Connection: Upgrade\r\n` +
        `Sec-WebSocket-Key: ${key}\r\n` +
        `Sec-WebSocket-Version: 13\r\n` +
        `Origin: http://${HOST}\r\n` +
        `\r\n`
    );
}

function buildFrame({opcode, fin, payload}) {
    const p = Buffer.isBuffer(payload) ? payload : Buffer.from(payload);
    const maskKey = crypto.randomBytes(4);       // client must mask
    const maskedBit = 0x80;

    const b0 = (fin ? 0x80 : 0x00) | (opcode & 0x0f);

    let b1 = maskedBit;
    let ext = Buffer.alloc(0);
    if (p.length < 126) {
        b1 |= p.length;
    } else if (p.length <= 0xffff) {
        b1 |= 126;
        ext = Buffer.alloc(2);
        ext.writeUInt16BE(p.length, 0);
    } else {
        b1 |= 127;
        ext = Buffer.alloc(8);
        const hi = Math.floor(p.length / 2 ** 32);
        const lo = p.length >>> 0;
        ext.writeUInt32BE(hi, 0);
        ext.writeUInt32BE(lo, 4);
    }

    // apply mask
    const masked = Buffer.alloc(p.length);
    for (let i = 0; i < p.length; i++) {
        masked[i] = p[i] ^ maskKey[i & 3];
    }

    return Buffer.concat([
        Buffer.from([b0, b1]),
        ext,
        maskKey,
        masked,
    ]);
}

const OPC = {CONT: 0x0, TEXT: 0x1, BINARY: 0x2, CLOSE: 0x8};

function sendFragmentedText(sock, totalBytes = 3400, frag1 = 500, frag2 = 1500) {
    // build a deterministic payload of 'X's
    const payload = Buffer.alloc(totalBytes, 0x58);

    const a = payload.subarray(0, frag1);
    const b = payload.subarray(frag1, frag1 + frag2);
    const c = payload.subarray(frag1 + frag2); // rest

    const f1 = buildFrame({opcode: OPC.TEXT, fin: false, payload: a}); // start
    const f2 = buildFrame({opcode: OPC.CONT, fin: false, payload: b}); // middle
    const f3 = buildFrame({opcode: OPC.CONT, fin: true, payload: c}); // final

    sock.write(f1);
    console.log('→ sent TEXT FIN=0 (part 1)');
    sock.write(f2);
    console.log('→ sent CONT FIN=0 (part 2)');
    sock.write(f3);
    console.log('→ sent CONT FIN=1 (final)');
}

function gracefulClose(sock, code = 1000, reason = 'bye') {
    // client close frame must be masked
    const closePayload = Buffer.alloc(2 + Buffer.byteLength(reason));
    closePayload.writeUInt16BE(code, 0);
    closePayload.write(reason, 2);
    const close = buildFrame({opcode: OPC.CLOSE, fin: true, payload: closePayload});
    sock.write(close);
    console.log('→ sent CLOSE');
}

const sock = net.createConnection({host: HOST, port: PORT}, () => {
    sock.write(makeHandshake());
});

let upgraded = false;
let rx = Buffer.alloc(0);
let weSentClose = false;

sock.on('data', (chunk) => {
    if (!upgraded) {
        rx = Buffer.concat([rx, chunk]);
        const s = rx.toString('utf8');
        if (s.includes('\r\n\r\n')) {
            if (!/^HTTP\/1\.1 101 /.test(s)) {
                console.error('Handshake failed:\n', s);
                sock.end();
                return;
            }
            upgraded = true;
            console.log('✓ Handshake 101 Switching Protocols');

            // send fragmented text
            sendFragmentedText(sock, 3400, 500, 1500);

            // close a bit later
            setTimeout(() => { gracefulClose(sock); weSentClose = true; }, 300);

            rx = Buffer.alloc(0);
        }
        return;
    }

    rx = Buffer.concat([rx, chunk]);
    parseFrames();
});

function parseFrames() {
    while (rx.length >= 2) {
        const b0 = rx[0];
        const b1 = rx[1];
        const fin = !!(b0 & 0x80);
        const opcode = b0 & 0x0f;
        const masked = !!(b1 & 0x80);   // server frames must be unmasked (false)
        let len = b1 & 0x7f;
        let offset = 2;

        if (len === 126) {
            if (rx.length < offset + 2) return;
            len = rx.readUInt16BE(offset); offset += 2;
        } else if (len === 127) {
            if (rx.length < offset + 8) return;
            const hi = rx.readUInt32BE(offset);
            const lo = rx.readUInt32BE(offset + 4);
            offset += 8;
            len = hi * 2 ** 32 + lo;
        }

        if (masked) {
            console.error('Protocol error: server frame is masked');
            sock.destroy();
            return;
        }

        if (rx.length < offset + len) return; // wait for full payload

        const payload = rx.subarray(offset, offset + len);
        rx = rx.subarray(offset + len); // consume frame

        if (opcode === 0x8) { // CLOSE
            // If payload has at least 2 bytes, it’s a status code
            let code = null, reason = '';
            if (payload.length >= 2) {
                code = payload.readUInt16BE(0);
                reason = payload.subarray(2).toString('utf8');
            }
            console.log(`← CLOSE from server${code ? ` (${code}${reason ? `, "${reason}"` : ''})` : ''}`);

            // We initiated the close: finish by closing TCP
            // (If we hadn’t sent a close, we would send one back here first.)
            sock.end(); // graceful FIN
            return;
        } else if (opcode === 0x1 || opcode === 0x2 || opcode === 0x0) {
            // TEXT / BINARY / CONT — you can ignore or parse further
            console.log(`← frame opcode=${opcode} fin=${fin} len=${payload.length}`);
        } else if (opcode === 0x9) {
            // PING → respond with PONG (same payload)
            // (not needed in your "no ping" script)
        } else if (opcode === 0xA) {
            // PONG — ignore
        } else {
            console.log(`← unknown opcode ${opcode}`);
        }
    }
}


sock.on('close', () => console.log('socket closed'));
sock.on('end', () => console.log('socket ended'));
sock.on('error', (e) => console.error('socket error:', e));

#!/usr/bin/env python3
"""
Bench-test sender for the ESP32 iBUS bridge.

Sends the binary ControlPacket format at 50Hz so you can verify the
ESP32 -> iBUS -> flight controller chain BEFORE writing/using a phone
app. Connect your computer to the ESP32's WiFi AP first.

PROPS OFF for any test where you're actually watching FC behavior.

IMPORTANT: --key must match UDP_AUTH_KEY in config.h EXACTLY, or every
packet will be silently rejected by the ESP32 (see its /status page's
"rejected" counter, or serial log, to check).

Usage:
    python3 test_udp_sender.py --key "your-secret-here"
    python3 test_udp_sender.py --key "your-secret-here" --throttle 1200
    python3 test_udp_sender.py --key "your-secret-here" --aux1 on
"""

import argparse
import hashlib
import hmac
import socket
import struct
import time

ESP32_IP = "192.168.4.1"   # default ESP32 softAP address
PORT = 1234
MAGIC = 0xA5
TAG_LEN = 8   # must match UDP_AUTH_TAG_LEN in config.h


def build_packet(key: bytes, seq, roll, pitch, throttle, yaw, aux_mask):
    # <  little-endian, no padding (matches #pragma pack(push,1) on the ESP32)
    # B magic | I seq(uint32) | H roll | H pitch | H throttle | H yaw | B auxMask
    body = struct.pack("<BIHHHHB", MAGIC, seq & 0xFFFFFFFF, roll, pitch, throttle, yaw, aux_mask)
    tag = hmac.new(key, body, hashlib.sha256).digest()[:TAG_LEN]
    return body + tag


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", default=ESP32_IP)
    ap.add_argument("--key", required=True,
                     help="Must match UDP_AUTH_KEY in config.h exactly")
    ap.add_argument("--roll", type=int, default=1500)
    ap.add_argument("--pitch", type=int, default=1500)
    ap.add_argument("--throttle", type=int, default=1000)
    ap.add_argument("--yaw", type=int, default=1500)
    ap.add_argument("--aux1", choices=["on", "off"], default="off")
    ap.add_argument("--aux2", choices=["on", "off"], default="off")
    ap.add_argument("--aux3", choices=["on", "off"], default="off")
    ap.add_argument("--aux4", choices=["on", "off"], default="off")
    ap.add_argument("--rate", type=float, default=50.0, help="packets per second")
    args = ap.parse_args()

    key = args.key.encode("utf-8")

    aux_mask = 0
    aux_mask |= 0x01 if args.aux1 == "on" else 0
    aux_mask |= 0x02 if args.aux2 == "on" else 0
    aux_mask |= 0x04 if args.aux3 == "on" else 0
    aux_mask |= 0x08 if args.aux4 == "on" else 0

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Seed from wall-clock milliseconds (mod 2^32) instead of 0, so that
    # re-running this script never sends a seq the ESP32 has already
    # seen from a previous run - the anti-replay check on the receiver
    # requires seq to strictly increase and will otherwise reject every
    # packet until the ESP32 itself is rebooted (or its link-loss reset
    # kicks in after FAILSAFE_TIMEOUT_MS of silence).
    seq = int(time.time() * 1000) & 0xFFFFFFFF
    period = 1.0 / args.rate

    print(f"Sending to {args.ip}:{PORT} at {args.rate} Hz. Ctrl+C to stop.")
    print(f"roll={args.roll} pitch={args.pitch} throttle={args.throttle} "
          f"yaw={args.yaw} auxMask={aux_mask:#04x}")

    try:
        while True:
            pkt = build_packet(key, seq, args.roll, args.pitch, args.throttle, args.yaw, aux_mask)
            sock.sendto(pkt, (args.ip, PORT))
            seq += 1
            time.sleep(period)
    except KeyboardInterrupt:
        print("\nStopped.")


if __name__ == "__main__":
    main()
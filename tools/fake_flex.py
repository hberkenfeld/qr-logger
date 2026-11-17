#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Minimal FlexRadio emulator for developing QR-Logger without a radio.

Emulates discovery broadcasts (UDP) and the TCP command channel with one
GUI client and two slices. Not a complete implementation of the API.

    python3 tools/fake_flex.py [--port 4992] [--discovery-target 127.0.0.1]
"""
import argparse
import socket
import socketserver
import struct
import threading
import time

GUI_HANDLE = "0x1A2B3C4D"
GUI_ID = "72E8C7F3-5766-4ADF-9286-EBF2F525C770"
SLICES = [
    "slice 0 in_use=1 RF_frequency=14.025000 mode=CW index_letter=A tx=1 active=1 client_handle=" + GUI_HANDLE,
    "slice 1 in_use=1 RF_frequency=7.012500 mode=CW index_letter=B tx=0 active=0 client_handle=" + GUI_HANDLE,
]


def discovery_packet(tcp_port):
    payload = (f"model=FLEX-6700 serial=0000-0000-6700-TEST version=3.10.15.0 "
               f"nickname=Fake\x7fFlex callsign=DF1QR ip=127.0.0.1 port={tcp_port} status=Available "
               f"gui_client_programs=AetherSDR gui_client_stations=Shack gui_client_handles={GUI_HANDLE}").encode()
    payload += b"\0" * (4 - len(payload) % 4)
    words = 7 + len(payload) // 4
    header = (0x3 << 28) | (1 << 27) | (0x3 << 22) | (0x1 << 20) | words
    return struct.pack(">IIIIIII", header, 0x800, 0x00001C2D, 0x534CFFFF, 0, 0, 0) + payload


def discovery_loop(target, udp_port, tcp_port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    while True:
        sock.sendto(discovery_packet(tcp_port), (target, udp_port))
        time.sleep(1)


class Handler(socketserver.StreamRequestHandler):
    def send(self, line):
        self.wfile.write((line + "\n").encode())

    def handle(self):
        handle = "5EED0001"
        self.send("V1.4.0.0")
        self.send("H" + handle)
        for raw in self.rfile:
            line = raw.decode(errors="replace").strip()
            if not line.startswith("C") or "|" not in line:
                continue
            seq, cmd = line[1:].split("|", 1)
            seq = seq.lstrip("D")
            if cmd != "ping":
                print("<-", cmd, flush=True)
            if cmd.startswith("client program"):
                self.send(f"R{seq}|10000002|unknown client program")
                continue
            if cmd == "info":
                self.send(f'R{seq}|0|model="FLEX-6700",chassis_serial="0000-0000-6700-TEST",name="Fake Flex",'
                          f'callsign="DF1QR",num_scu=2,num_slice=8,num_tx=1,software_ver=4.2.20.41343')
            elif cmd.startswith("client bind"):
                ok = cmd.endswith("client_id=" + GUI_ID)
                self.send(f"R{seq}|{'0' if ok else '50000016'}|")
            else:
                self.send(f"R{seq}|0|")
            if cmd == "sub client all":
                self.send(f"S0|client {GUI_HANDLE} connected local_ptt=1 client_id={GUI_ID} "
                          f"program=AetherSDR station=Shack")
                self.send("S0|client 0x0BADBEEF connected program=SmartSDR-CAT")
            elif cmd == "sub slice all":
                for s in SLICES:
                    self.send(f"S{GUI_HANDLE[2:]}|{s}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=4992, help="TCP port")
    ap.add_argument("--discovery-port", type=int, default=4992)
    ap.add_argument("--discovery-target", default="127.0.0.1")
    args = ap.parse_args()
    threading.Thread(target=discovery_loop,
                     args=(args.discovery_target, args.discovery_port, args.port), daemon=True).start()
    socketserver.ThreadingTCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer(("0.0.0.0", args.port), Handler) as srv:
        print(f"Fake FLEX-6700 on TCP {args.port}", flush=True)
        srv.serve_forever()


if __name__ == "__main__":
    main()

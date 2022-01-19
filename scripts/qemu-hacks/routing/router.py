# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

import os
import time
from socket import socket, AF_INET, SOCK_DGRAM, SOL_SOCKET, SO_REUSEADDR
import sys
from threading import Thread
import ipaddress
import signal
import fcntl
import struct
from os.path import exists as file_exists

PAYLOAD_OFFSET = 14

ARP_PROTO = 0x0806
ARP_HTYPE = 1
ARP_PTYPE = 0x0800
ARP_HLEN = 6
ARP_PLEN = 4
ARP_REQ = 1
ARP_ANS = 2
ARP_PAYLOAD_OFFSET = 8

IP_PROTO = ARP_PTYPE
IP_DST_IP_OFFSET = 16

ip_mac = dict()
mac_ip = dict()
target_qemu = dict()


tunnel_socket = socket(AF_INET, SOCK_DGRAM)
server_address = ("127.0.0.1", 16384)
tunnel_socket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
tunnel_socket.bind(server_address)
tunnel_socket.settimeout(1)

tun_fd = os.open("/dev/tap0", os.O_RDWR)
out_strm = os.popen("ifconfig tap0 | grep ether |cut -dH -f2|cut -d\  -f2")
mac_str = out_strm.read().strip()
out_strm.close()

print("router mac %s" % (mac_str,))

router_ip_int = int(ipaddress.IPv4Address("192.168.122.1"))
router_ip_max = int(ipaddress.IPv4Address("192.168.122.255"))

router_mac = int(mac_str.replace(':', ''), 16)

ip_mac[router_ip_int] = router_mac
mac_ip[router_mac] = router_ip_int


ROUTER_NOTSTOP = True

if file_exists("qemus.txt"):
    old_qemus = open("qemus.txt", "r")
    old_qemus_lines = old_qemus.readlines()
    old_qemus.close()

    for line in old_qemus_lines:
        parts = line.split(":")

        mac = int(parts[0]).to_bytes(6, "big")

        print("mac is %s at %s:%s" % (':'.join('%02x' % b for b in mac), parts[1], parts[2],))

        target_qemu[int(parts[0])] = (parts[1], int(parts[2]))
else:
    print("no old mac mapping")


def router_stop(signum, frame):
    global ROUTER_NOTSTOP
    global tun_fd
    ROUTER_NOTSTOP = False
    os.close(tun_fd)

    qemus = open("qemus.txt", "w")

    for k in target_qemu:
        v = target_qemu[k]
        qemus.write("%i:%s:%i\n" % (k, v[0], v[1]))

    qemus.close()


signal.signal(signal.SIGTERM, router_stop)
signal.signal(signal.SIGINT, router_stop)


def arp_parse_reply(ts, addr, data):

    if router_mac is None:
        return

    if int.from_bytes(data[PAYLOAD_OFFSET + 6:PAYLOAD_OFFSET + 8], "big") != ARP_REQ:
        return

    src_mac_start = PAYLOAD_OFFSET + ARP_PAYLOAD_OFFSET
    src_mac = data[src_mac_start:src_mac_start + 6]
    src_mac_int = int.from_bytes(src_mac, "big")

    src_ip_start = src_mac_start + 6
    src_ip = data[src_ip_start:src_ip_start + 4]
    src_ip_int = int.from_bytes(src_ip, "big")

    ip_mac[src_ip_int] = src_mac_int
    mac_ip[src_mac_int] = src_ip_int

    tgt_ip_start = src_ip_start + 4 + 6
    tgt_ip = data[tgt_ip_start:tgt_ip_start + 4]
    tgt_ip_int = int.from_bytes(tgt_ip, "big")

    if tgt_ip_int != router_ip_int:
        return

    ar = data[0:PAYLOAD_OFFSET]
    ar += ARP_HTYPE.to_bytes(2, "big")
    ar += ARP_PTYPE.to_bytes(2, "big")
    ar += ARP_HLEN.to_bytes(1, "big")
    ar += ARP_PLEN.to_bytes(1, "big")
    ar += ARP_ANS.to_bytes(2, "big")
    ar += ip_mac.get(tgt_ip_int).to_bytes(6, "big")
    ar += data[tgt_ip_start:tgt_ip_start + 4]
    ar += data[src_mac_start:src_mac_start + 6]
    ar += data[src_ip_start:src_ip_start + 4]

    try:
        os.write(tun_fd, ar)
    except Exception as e:
        pass

    try:
        ts.sendto(ar, addr)
    except Exception as e:
        pass


def tap_router():
    global tun_fd
    global ip_mac
    global mac_ip
    global target_qemu
    global tunnel_socket
    global router_mac
    global ROUTER_NOTSTOP

    while ROUTER_NOTSTOP:
        try:
            data = None

            try:
                data = os.read(tun_fd, 65536)
            except Exception as e:
                pass

            if data is None:
                time.sleep(0.1)
                continue

            dst_mac = ':'.join('%02x' % b for b in data[0:6])
            src_mac = ':'.join('%02x' % b for b in data[6:12])
            protocol = int.from_bytes(data[12:14], "big")

            try:
                os.write(tun_fd, data)
            except Exception as e:
                pass

            if protocol == IP_PROTO:
                dst_ip_bytes = data[PAYLOAD_OFFSET + IP_DST_IP_OFFSET:PAYLOAD_OFFSET + IP_DST_IP_OFFSET + 4]
                dst_ip_int = int.from_bytes(dst_ip_bytes, "big")

                if dst_ip_int > router_ip_int and dst_ip_int < router_ip_max:
                    dst_mac_int = int.from_bytes(data[0:6], "big")

                    if target_qemu.get(dst_mac_int):
                        tunnel_socket.sendto(data, target_qemu.get(dst_mac_int))

                elif dst_ip_int == router_ip_max:
                    for addr in target_qemu:
                        tunnel_socket.sendto(data, target_qemu[addr])

            elif protocol == ARP_PROTO:
                if dst_mac == "ff:ff:ff:ff:ff:ff":
                    for addr in target_qemu:
                        tunnel_socket.sendto(data, target_qemu[addr])

        except Exception as e:
            print(e)

    print("tap router stopped")


def qemu_handler():
    global ROUTER_NOTSTOP
    global tunnel_socket
    global target_qemu

    while ROUTER_NOTSTOP:
        try:
            data, address = tunnel_socket.recvfrom(65536)
            dst_mac = ':'.join('%02x' % b for b in data[0:6])
            src_mac = ':'.join('%02x' % b for b in data[6:12])
            protocol = int.from_bytes(data[12:14], "big")

            src_mac_int = int.from_bytes(data[6:12], "big")
            target_qemu[src_mac_int] = address

            if protocol == ARP_PROTO:
                arp_parse_reply(tunnel_socket, address, data)

            try:
                os.write(tun_fd, data)
            except Exception as e:
                pass
        except Exception as e:
            pass

    print("qemu handler stopped")


th_tr = Thread(target=tap_router)
th_tr.start()

th_qh = Thread(target=qemu_handler)
th_qh.start()

th_tr.join()
th_qh.join()

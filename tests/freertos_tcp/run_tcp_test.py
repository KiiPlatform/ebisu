#!/usr/bin/env python3
"""Runs the tests of the socket over FreeRTOS-Plus-TCP in QEMU, against servers
of this script's on the host's loopback, which QEMU's user-mode networking
reaches from the firmware at 10.0.2.2. Their ports go to the firmware on its
command line, through semihosting. Exits with QEMU's status: 0 if every check
passed.

    run_tcp_test.py <qemu-system-arm> <freertos_tcp_test.elf>
"""

import socket
import struct
import subprocess
import sys
import threading
import time


def serve(listener, handle):
    """Accepts connections on listener, each handled on a thread of its own."""
    def accept():
        while True:
            try:
                peer, _ = listener.accept()
            except OSError:
                return
            threading.Thread(target=handle, args=(peer,), daemon=True).start()
    threading.Thread(target=accept, daemon=True).start()


def until_closed(peer):
    """Reads until the firmware closes the connection, then closes it too."""
    try:
        while peer.recv(256):
            pass
    except OSError:
        pass
    peer.close()


def echo(peer):
    try:
        while True:
            data = peer.recv(256)
            if not data:
                break
            peer.sendall(data)
    except OSError:
        pass
    peer.close()


def late(peer):
    """Sends 3 seconds after the connection."""
    time.sleep(3)
    try:
        peer.sendall(b"late")
    except OSError:
        pass
    until_closed(peer)


def partial(peer):
    peer.sendall(b"abcdef")
    until_closed(peer)


def closer(peer):
    peer.sendall(b"bye")
    peer.shutdown(socket.SHUT_WR)
    until_closed(peer)


def reset(peer):
    # Closed with a linger of 0: a reset.
    peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
    time.sleep(0.5)
    peer.close()


def listen():
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(8)
    return listener


def main():
    qemu, firmware = sys.argv[1], sys.argv[2]
    ports = {}
    listeners = []
    for name, handle in (("echo", echo), ("late", late), ("partial", partial),
                         ("closer", closer), ("reset", reset)):
        listener = listen()
        listeners.append(listener)
        ports[name] = listener.getsockname()[1]
        serve(listener, handle)
    # A port nothing listens on: one that was free a moment ago. QEMU's
    # user-mode networking leaves a connection to it unanswered.
    unused = listen()
    ports["unanswered"] = unused.getsockname()[1]
    unused.close()

    args = ",".join("arg=%s=%d" % item for item in ports.items())
    command = [qemu, "-machine", "mps2-an385", "-cpu", "cortex-m3",
               "-display", "none", "-monitor", "none", "-serial", "stdio",
               "-semihosting-config", "enable=on,target=native,arg=tcp_test," + args,
               "-netdev", "user,id=net0", "-net", "nic,model=lan9118,netdev=net0",
               "-kernel", firmware]
    try:
        status = subprocess.run(command, timeout=300).returncode
    except subprocess.TimeoutExpired:
        print("run_tcp_test: FAILED: QEMU ran out of time", flush=True)
        status = 1
    for listener in listeners:
        listener.close()
    return status


if __name__ == "__main__":
    sys.exit(main())

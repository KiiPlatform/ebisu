#!/usr/bin/env python3
"""Runs the tests of the socket and the name servers over FreeRTOS-Plus-TCP in
QEMU, against servers of this script's on the host's loopback, TCP ones and
name servers over UDP, which QEMU's user-mode networking reaches from the
firmware at 10.0.2.2. Their ports go to the firmware on its command line,
through semihosting. Exits with QEMU's status: 0 if every check passed.

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


def reply(query, rcode):
    """The query as its answer, with rcode and no records: what any name server
    sends for a query it takes, if only to say that the name doesn't exist."""
    return query[:2] + bytes([query[2] | 0x80, 0x80 | rcode]) + query[4:]


def stray(query):
    """An answer to another query: of another identifier."""
    answer = reply(query, 0)
    return bytes([answer[0] ^ 0xff]) + answer[1:]


def answers(server, query, peer):
    server.sendto(reply(query, 0), peer)


def no_such_name(server, query, peer):
    server.sendto(reply(query, 3), peer)


def silent(server, query, peer):
    pass


def stray_first(server, query, peer):
    """An answer to another query, then, 300 ms later, the answer."""
    server.sendto(stray(query), peer)
    time.sleep(0.3)
    server.sendto(reply(query, 0), peer)


def strays(server, query, peer):
    """Only answers to other queries, one every 200 ms for a second."""
    for _ in range(5):
        server.sendto(stray(query), peer)
        time.sleep(0.2)


# Where elsewhere answers from.
other = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
other.bind(("127.0.0.1", 0))


def elsewhere(server, query, peer):
    """The answer, but from another port."""
    other.sendto(reply(query, 0), peer)


def serve_udp(handle):
    """A name server on the loopback, each query handled by handle, in turn."""
    server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.bind(("127.0.0.1", 0))

    def receive():
        while True:
            try:
                query, peer = server.recvfrom(512)
            except OSError:
                return
            if len(query) >= 12:
                handle(server, query, peer)
    threading.Thread(target=receive, daemon=True).start()
    return server


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
    for name, handle in (("dns", answers), ("nxdomain", no_such_name),
                         ("silent", silent), ("stray", stray_first),
                         ("strays", strays), ("elsewhere", elsewhere)):
        server = serve_udp(handle)
        listeners.append(server)
        ports[name] = server.getsockname()[1]
    # A port nothing listens on: one that was free a moment ago. QEMU's
    # user-mode networking leaves a connection to it unanswered.
    unused = listen()
    ports["unanswered"] = unused.getsockname()[1]
    unused.close()
    # One for UDP, whose datagrams the host refuses.
    unused = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    unused.bind(("127.0.0.1", 0))
    ports["refused"] = unused.getsockname()[1]
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

#!/usr/bin/env python3
"""Tests the win32 sample's socket callbacks, sock_cb_win32.c, under wine.

Runs sock_cb_test.exe against TLS servers on 127.0.0.1 that this script
controls down to how the ciphertext is cut into TCP segments: large and
fragmented records, receive timeouts, resets, close_notify, the end of the TCP
stream, and certificates that must be refused.

Wine's root store is the CA bundle of the system it runs on, so the test makes
a CA of its own with the openssl command and a wine prefix of its own, whose
root store it adds the CA to. Nothing outside a temporary directory is touched.

    tls_test.py <sock_cb_test.exe> [--wine <wine>] [--workdir <directory>]
"""
import argparse
import os
import random
import re
import shutil
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time

TIMEOUT = 120


def fnv1a(data):
    h = 0xcbf29ce484222325
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return '%016x' % h


def openssl(cwd, *args):
    subprocess.run(['openssl'] + list(args), cwd=cwd, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


CA_CONFIG = '''
[ ca ]
default_ca = test_ca
[ test_ca ]
database = index.txt
new_certs_dir = .
serial = serial
default_md = sha256
policy = policy_any
copy_extensions = copy
unique_subject = no
[ policy_any ]
commonName = supplied
[ v3_ca ]
basicConstraints = critical, CA:TRUE
keyUsage = critical, keyCertSign, cRLSign
subjectKeyIdentifier = hash
[ server ]
basicConstraints = critical, CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
'''


def make_certificates(d):
    """A CA, and server certificates: good, larger than a TLS record, issued
    for another name, expired, and self-signed."""
    with open(os.path.join(d, 'ca.cnf'), 'w') as f:
        f.write(CA_CONFIG)
    open(os.path.join(d, 'index.txt'), 'w').close()
    with open(os.path.join(d, 'serial'), 'w') as f:
        f.write('1000\n')
    openssl(d, 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-keyout', 'ca.key', '-out', 'ca.pem',
            '-days', '2', '-subj', '/CN=ebisu win32 sample test CA', '-config', 'ca.cnf', '-extensions', 'v3_ca')
    openssl(d, 'x509', '-in', 'ca.pem', '-outform', 'der', '-out', 'ca.der')

    def issue(name, dns, dates, more_names=()):
        san = ','.join('DNS:' + n for n in [dns] + list(more_names))
        openssl(d, 'req', '-newkey', 'rsa:2048', '-nodes', '-keyout', name + '.key', '-out', name + '.csr',
                '-subj', '/CN=' + dns, '-addext', 'subjectAltName=' + san)
        # openssl ca rather than x509 -req, which cannot backdate.
        openssl(d, 'ca', '-batch', '-config', 'ca.cnf', '-cert', 'ca.pem', '-keyfile', 'ca.key',
                '-in', name + '.csr', '-out', name + '.pem', '-extensions', 'server', '-notext', *dates)

    issue('good', 'localhost', ['-days', '2'])
    # Some 38 kB, so that the Certificate message spans three records.
    issue('big', 'localhost', ['-days', '2'], ['name%04d.some-long-subdomain.example.org' % i for i in range(900)])
    issue('wrongname', 'wrong.example', ['-days', '2'])
    issue('expired', 'localhost', ['-startdate', '20200101000000Z', '-enddate', '20200201000000Z'])
    openssl(d, 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-keyout', 'self.key', '-out', 'self.pem',
            '-days', '2', '-subj', '/CN=localhost', '-addext', 'subjectAltName=DNS:localhost')


def payload(n, seed):
    return random.Random(seed).randbytes(n)


def alphabet(n):
    return bytes(ord('a') + i % 26 for i in range(n))


class TlsConnection:
    """The server end of one connection, over memory BIOs, so that the
    ciphertext goes out in whatever pieces a test asks for."""

    def __init__(self, sock, ctx):
        self.sock = sock
        self.incoming = ssl.MemoryBIO()
        self.outgoing = ssl.MemoryBIO()
        self.tls = ctx.wrap_bio(self.incoming, self.outgoing, server_side=True)
        self.plain = b''

    def flush(self):
        data = self.outgoing.read()
        if data:
            self.sock.sendall(data)

    def feed(self):
        data = self.sock.recv(65536)
        if not data:
            raise EOFError('end of TCP stream')
        self.incoming.write(data)

    def handshake(self):
        while True:
            try:
                self.tls.do_handshake()
                break
            except ssl.SSLWantReadError:
                self.flush()
                self.feed()
        self.flush()

    def read_until(self, done):
        """Plaintext until done(plain) holds. None for close_notify."""
        while not done(self.plain):
            try:
                data = self.tls.read(65536)
            except ssl.SSLWantReadError:
                self.flush()
                self.feed()
                continue
            except ssl.SSLZeroReturnError:
                return None
            if not data:
                return None
            self.plain += data
        return self.plain

    def readline(self):
        if self.read_until(lambda p: b'\n' in p) is None:
            return None
        line, self.plain = self.plain.split(b'\n', 1)
        return line.decode()

    def read_exact(self, n):
        if self.read_until(lambda p: len(p) >= n) is None:
            raise EOFError('close_notify')
        data, self.plain = self.plain[:n], self.plain[n:]
        return data

    def encrypt(self, data):
        self.flush()
        self.tls.write(data)
        return self.outgoing.read()

    def send_close_notify(self, before=b''):
        try:
            self.tls.unwrap()
        except ssl.SSLWantReadError:
            pass
        self.sock.sendall(before + self.outgoing.read())

    def wait_for_client_close(self):
        """How the client ended: 'close_notify', or 'eof' without one."""
        try:
            while True:
                try:
                    if self.tls.read(65536) == b'':
                        return 'close_notify'
                except ssl.SSLWantReadError:
                    self.feed()
                except ssl.SSLZeroReturnError:
                    return 'close_notify'
        except EOFError:
            return 'eof'


def split_records(ciphertext):
    records = []
    while ciphertext:
        n = 5 + struct.unpack('>H', ciphertext[3:5])[0]
        records.append(ciphertext[:n])
        ciphertext = ciphertext[n:]
    return records


def send_pieces(sock, data, mode, rng):
    if mode == 'whole':
        sock.sendall(data)
    elif mode == 'byte':
        for i in range(len(data)):
            sock.sendall(data[i:i + 1])
    elif mode == 'random':
        pos = 0
        while pos < len(data):
            n = rng.randint(1, 3000)
            sock.sendall(data[pos:pos + n])
            pos += n
            if rng.random() < 0.05:
                time.sleep(0.002)
    elif mode == 'halves':
        for record in split_records(data):
            sock.sendall(record[:len(record) // 2])
            time.sleep(0.01)
            sock.sendall(record[len(record) // 2:])
    else:
        raise ValueError(mode)


def serve_commands(conn):
    """Carries out the commands the client sends, a line each, and returns how
    the connection ended."""
    sock = conn.sock
    while True:
        line = conn.readline()
        if line is None:
            return 'close_notify'
        cmd, _, arg = line.partition(' ')
        if cmd == 'echo':
            sock.sendall(conn.encrypt(arg.encode() + b'\n'))
        elif cmd == 'sleep':
            time.sleep(int(arg) / 1000)
        elif cmd == 'partial':
            # One record, in two halves, with a pause between.
            ms, text = arg.split(' ', 1)
            data = conn.encrypt(text.encode() + b'\n')
            sock.sendall(data[:len(data) // 2])
            time.sleep(int(ms) / 1000)
            sock.sendall(data[len(data) // 2:])
        elif cmd == 'big':
            size, mode, seed = arg.split()
            send_pieces(sock, conn.encrypt(payload(int(size), int(seed))), mode, random.Random(seed))
        elif cmd == 'digest':
            data = conn.read_exact(int(arg))
            sock.sendall(conn.encrypt(('%d %s\n' % (len(data), fnv1a(data))).encode()))
        elif cmd == 'close_notify':
            # The data and close_notify in one segment, then wait for the
            # client's close_notify, as a server closing cleanly does,
            # without ending the TCP stream first.
            conn.send_close_notify(conn.encrypt(arg.encode() + b'\n') if arg else b'')
            return conn.wait_for_client_close()
        elif cmd == 'fin':
            # The end of the TCP stream, without close_notify.
            sock.shutdown(socket.SHUT_WR)
            return conn.wait_for_client_close()
        elif cmd == 'fin_mid':
            data = conn.encrypt(arg.encode() + b'\n')
            sock.sendall(data[:len(data) // 2])
            sock.shutdown(socket.SHUT_WR)
            return conn.wait_for_client_close()
        elif cmd == 'reset':
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
            return 'reset'
        else:
            raise ValueError(cmd)


class Server:
    """Listens on 127.0.0.1 and records how each connection went."""

    def __init__(self, cert=None, key=None, silent=False):
        self.ctx = None
        if cert:
            self.ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            self.ctx.load_cert_chain(cert, key)
        self.silent = silent
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(16)
        self.port = self.listener.getsockname()[1]
        self.ends = []
        self.versions = []
        self.lock = threading.Lock()
        threading.Thread(target=self.accept, daemon=True).start()

    def accept(self):
        while True:
            sock, _ = self.listener.accept()
            threading.Thread(target=self.handle, args=(sock,), daemon=True).start()

    def handle(self, sock):
        end = None
        try:
            if self.silent:
                # Takes the ClientHello, and never answers it.
                sock.settimeout(TIMEOUT)
                while sock.recv(65536):
                    pass
                end = 'eof'
                return
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            conn = TlsConnection(sock, self.ctx)
            conn.handshake()
            with self.lock:
                self.versions.append(conn.tls.version())
            end = serve_commands(conn)
        except (EOFError, ConnectionError, ssl.SSLError, socket.timeout) as e:
            end = 'error: %r' % e
        finally:
            sock.close()
            with self.lock:
                self.ends.append(end)

    def end(self, n, wait=10):
        """How connection n, counting from 0, ended."""
        deadline = time.time() + wait
        while time.time() < deadline:
            with self.lock:
                if len(self.ends) > n:
                    return self.ends[n]
            time.sleep(0.05)
        return None


class Test:
    def __init__(self, exe, wine, prefix):
        self.exe = exe
        self.wine = wine
        self.env = dict(os.environ, WINEPREFIX=prefix, WINEDEBUG='-all')
        self.failures = 0

    def run(self, port, to_recv, *steps):
        """The output of sock_cb_test.exe, and the STEP lines in it, split
        into the step and the rest."""
        out = subprocess.run([self.wine, self.exe, 'localhost', str(port), str(to_recv)] + list(steps),
                             env=self.env, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             text=True, timeout=TIMEOUT).stdout.replace('\r', '')
        steps = [(line.split(' ', 2) + [''])[1:3] for line in out.splitlines() if line.startswith('STEP ')]
        return out, [tuple(s) for s in steps]

    def check(self, name, ok, out):
        print('%s: %s' % ('ok' if ok else 'FAILED', name), flush=True)
        if not ok:
            self.failures += 1
            print('    ' + out.rstrip().replace('\n', '\n    '), flush=True)


def field(result, key):
    m = re.search(r'\b%s=(\S*)' % key, result)
    return m.group(1) if m else None


def step(steps, i):
    return steps[i] if i < len(steps) else ('', '')


def recv_result(steps, i):
    """(code, length, timed_out, ms, data) of steps[i], a recv step, or a
    tuple that matches nothing."""
    name, rest = step(steps, i)
    if name != 'recv':
        return ('', -1, '', -1, '')
    code, length = rest.split(' ')[:2]
    return code, int(length), field(rest, 'timed_out'), int(field(rest, 'ms')), rest.split('data=', 1)[1]


def run_tests(t, d):
    good = Server(os.path.join(d, 'good.pem'), os.path.join(d, 'good.key'))
    port = good.port

    n = len(good.ends)
    out, steps = t.run(port, 5, 'connect', 'send:echo hello\\n', 'recv:100', 'close')
    t.check('round trip, TLS version %s' % (good.versions[-1] if good.versions else None),
            step(steps, 0) == ('connect', 'OK') and step(steps, 1) == ('send', 'OK 11') and
            recv_result(steps, 2)[::4] == ('OK', 'hello\\n') and step(steps, 3) == ('close', 'OK'), out)
    t.check('close sends close_notify', good.end(n) == 'close_notify', out + '\nserver: %s' % good.end(n))

    # (bytes, how the server cuts the ciphertext up, the client's buffer)
    for size, mode, bufsize in ((1000000, 'whole', 65536), (1000000, 'random', 4096),
                                (300000, 'halves', 100), (100000, 'byte', 1000), (70000, 'random', 1)):
        seed = size + bufsize
        out, steps = t.run(port, 10, 'connect', 'send:big %d %s %d\\nclose_notify\\n' % (size, mode, seed),
                           'drain:%d' % bufsize, 'close')
        drain = step(steps, 2)[1]
        t.check('%d bytes cut up %s, read %d at a time' % (size, mode, bufsize),
                drain.startswith('OK bytes=%d ' % size) and field(drain, 'fnv') == fnv1a(payload(size, seed)),
                out)

    out, steps = t.run(port, 10, 'connect', 'send:digest 200000\\n', 'sendbig:200000', 'recv:100', 'close')
    t.check('200000 bytes sent, in several records',
            step(steps, 2)[1].startswith('OK 200000 ') and
            recv_result(steps, 3)[4] == '200000 %s\\n' % fnv1a(alphabet(200000)), out)

    # The MQTT task's idle wait: the receive times out, and the connection is
    # still there for the next one. With a 2 s timeout, the data comes in the
    # middle of the third receive.
    out, steps = t.run(port, 2, 'connect', 'send:sleep 5000\\necho late\\n', 'recv:100', 'recv:100',
                       'recv:100', 'close')
    r = [recv_result(steps, i) for i in (2, 3, 4)]
    t.check('receive timeout: FAIL, timed out after %s ms, and the data still arrives' % [x[3] for x in r[:2]],
            r[0][:3] == ('FAIL', 0, '1') and r[1][:3] == ('FAIL', 0, '1') and
            all(1900 <= x[3] <= 4000 for x in r[:2]) and r[2][::4] == ('OK', 'late\\n'), out)

    out, steps = t.run(port, 2, 'connect', 'send:partial 5000 split record\\n', 'recv:100', 'recv:100',
                       'recv:100', 'close')
    r = [recv_result(steps, i) for i in (2, 3, 4)]
    t.check('receive timeout with half a record received keeps the half',
            r[0][:3] == ('FAIL', 0, '1') and r[1][:3] == ('FAIL', 0, '1') and
            r[2][::4] == ('OK', 'split record\\n'), out)

    out, steps = t.run(port, 10, 'connect', 'send:sleep 300\\nreset\\n', 'recv:100', 'close')
    r = recv_result(steps, 2)
    t.check('reset: FAIL, not timed out', r[:3] == ('FAIL', 0, '0') and r[3] < 5000, out)

    n = len(good.ends)
    out, steps = t.run(port, 10, 'connect', 'send:close_notify bye\\n', 'recv:100', 'recv:100', 'recv:100',
                       'close')
    r = [recv_result(steps, i) for i in (2, 3, 4)]
    t.check('close_notify: OK with 0 bytes, at once, and again',
            r[0][::4] == ('OK', 'bye\\n') and r[1][:3] == ('OK', 0, '0') and r[1][3] < 5000 and
            r[2][:3] == ('OK', 0, '0') and step(steps, 5) == ('close', 'OK'), out)
    t.check('close_notify answered with close_notify', good.end(n) == 'close_notify',
            out + '\nserver: %s' % good.end(n))

    out, steps = t.run(port, 10, 'connect', 'send:echo bye\\nfin\\n', 'recv:100', 'recv:100', 'recv:100',
                       'close')
    r = [recv_result(steps, i) for i in (2, 3, 4)]
    t.check('end of the TCP stream without close_notify: OK with 0 bytes',
            r[0][::4] == ('OK', 'bye\\n') and r[1][:3] == ('OK', 0, '0') and
            r[2][:3] == ('OK', 0, '0') and step(steps, 5) == ('close', 'OK'), out)

    out, steps = t.run(port, 10, 'connect', 'send:fin_mid truncated\\n', 'recv:100', 'close')
    r = recv_result(steps, 2)
    t.check('end of the TCP stream inside a record: FAIL, not timed out',
            r[:3] == ('FAIL', 0, '0'), out)

    big = Server(os.path.join(d, 'big.pem'), os.path.join(d, 'big.key'))
    out, steps = t.run(big.port, 10, 'connect', 'send:echo hello\\n', 'recv:100', 'close')
    t.check('certificate larger than a record',
            step(steps, 0) == ('connect', 'OK') and recv_result(steps, 2)[::4] == ('OK', 'hello\\n'), out)

    for name, error in (('self', '0x800b0109'), ('wrongname', '0x800b010f'), ('expired', '0x800b0101')):
        srv = Server(os.path.join(d, name + '.pem'), os.path.join(d, name + '.key'))
        out, steps = t.run(srv.port, 10, 'connect', 'close')
        t.check('certificate %s refused with %s' % (name, error),
                step(steps, 0) == ('connect', 'FAIL') and error in out, out)

    silent = Server(silent=True)
    out, steps = t.run(silent.port, 1, 'connect', 'close')
    t.check('server that never answers the ClientHello: connect FAILs after the receive timeout',
            step(steps, 0) == ('connect', 'FAIL') and 'timed out' in out, out)

    closed = socket.socket()
    closed.bind(('127.0.0.1', 0))
    closed_port = closed.getsockname()[1]
    closed.close()
    out, steps = t.run(closed_port, 1, 'connect', 'recv:10', 'close')
    t.check('connection refused: connect FAILs, and so does recv',
            step(steps, 0) == ('connect', 'FAIL') and recv_result(steps, 1)[:3] == ('FAIL', 0, '0') and
            step(steps, 2) == ('close', 'OK'), out)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('exe', help='sock_cb_test.exe')
    parser.add_argument('--wine', default='wine')
    parser.add_argument('--workdir', help='where to make the temporary directory, which holds a wine prefix '
                        'of some hundreds of megabytes; the system default if left out')
    args = parser.parse_args()

    if args.workdir:
        os.makedirs(args.workdir, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='ebisu-win32-tls-', dir=args.workdir)
    prefix = os.path.join(tmp, 'wineprefix')
    # Without Mono and Gecko, which wineboot would otherwise offer to install.
    env = dict(os.environ, WINEPREFIX=prefix, WINEDEBUG='-all', WINEDLLOVERRIDES='mscoree,mshtml=')
    try:
        make_certificates(tmp)
        subprocess.run([args.wine, 'wineboot', '-i'], env=env, check=True, timeout=600,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        out = subprocess.run([args.wine, args.exe, 'trust', os.path.join(tmp, 'ca.der')], env=env,
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
                             timeout=TIMEOUT).stdout
        if 'trusted' not in out:
            print('cannot add the test CA to the wine prefix: ' + out)
            return 1
        t = Test(args.exe, args.wine, prefix)
        run_tests(t, tmp)
        print('%d failed' % t.failures)
        return 1 if t.failures else 0
    finally:
        # The prefix's wineserver lingers for a few seconds after the last
        # process, holding the prefix.
        try:
            subprocess.run(['wineserver', '-k'], env=env, timeout=TIMEOUT,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except (OSError, subprocess.TimeoutExpired):
            pass
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""Runs a command with a Mosquitto broker of its own, for the broker tests.

The broker runs in a container, configured by mosquitto.conf, on a port
of 127.0.0.1 that the container tool picks. It speaks TLS only: its certificate,
for localhost, is issued by a CA the script makes with the openssl command, and
its one user's password is made for the run. The command finds them in its
environment:

    EBISU_BROKER_HOST      localhost
    EBISU_BROKER_PORT      the port
    EBISU_BROKER_CA        the CA's certificate, a PEM file
    EBISU_BROKER_USERNAME  the user
    EBISU_BROKER_PASSWORD  its password

If the command fails, the broker's log is printed. Then the container is
removed, and the temporary directory the files were made in.

    with_broker.py <command> [<argument>...]

DOCKER names the container tool, docker by default; DOCKER_OPTS adds options to
its run, such as --security-opt label=disable for podman with SELinux.
"""
import os
import secrets
import shlex
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import time

IMAGE = ('docker.io/library/eclipse-mosquitto:2.1.2-alpine'
         '@sha256:860aeb5d63ce509508f6dcfb2bc8a1c1dd2bc60555b562b45ce90e52d8dabb0f')
HERE = os.path.dirname(os.path.abspath(__file__))
USERNAME = 'ebisu'
# How long the broker may take to start.
START_TIMEOUT = 60


def openssl(cwd, *args):
    subprocess.run(['openssl'] + list(args), cwd=cwd, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def make_certificates(d):
    """A CA, and the broker's certificate for localhost, issued by it."""
    with open(os.path.join(d, 'server.ext'), 'w') as f:
        f.write('basicConstraints = critical, CA:FALSE\n'
                'keyUsage = critical, digitalSignature, keyEncipherment\n'
                'extendedKeyUsage = serverAuth\n'
                'subjectAltName = DNS:localhost, IP:127.0.0.1\n')
    openssl(d, 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-keyout', 'ca.key', '-out', 'ca.pem',
            '-days', '2', '-subj', '/CN=ebisu broker test CA',
            '-addext', 'basicConstraints=critical,CA:TRUE',
            '-addext', 'keyUsage=critical,keyCertSign,cRLSign')
    openssl(d, 'req', '-newkey', 'rsa:2048', '-nodes', '-keyout', 'server.key', '-out', 'server.csr',
            '-subj', '/CN=localhost')
    openssl(d, 'x509', '-req', '-in', 'server.csr', '-CA', 'ca.pem', '-CAkey', 'ca.key',
            '-CAcreateserial', '-days', '2', '-sha256', '-extfile', 'server.ext', '-out', 'server.pem')


def docker(*args, **kwargs):
    return subprocess.run([os.environ.get('DOCKER', 'docker')] + list(args), **kwargs)


def start_broker(d, name, password):
    """Starts the container, and returns the port it listens on."""
    # The files are copied out of the directory they are mounted from, and
    # made the broker's own, as it refuses a password file others can read.
    script = ('mkdir /tmp/broker && cp /work/mosquitto.conf /work/ca.pem'
              ' /work/server.pem /work/server.key /tmp/broker/'
              ' && mosquitto_passwd -b -c /tmp/broker/passwd "$BROKER_USERNAME" "$BROKER_PASSWORD"'
              ' && chown -R mosquitto:mosquitto /tmp/broker && chmod 0700 /tmp/broker'
              ' && chmod 0600 /tmp/broker/*'
              ' && exec mosquitto -c /tmp/broker/mosquitto.conf')
    env = dict(os.environ, BROKER_USERNAME=USERNAME, BROKER_PASSWORD=password)
    # The password goes in the container's environment from this one, rather
    # than on a command line.
    docker('run', '-d', '--name', name, *shlex.split(os.environ.get('DOCKER_OPTS', '')),
           '-p', '127.0.0.1::8883', '-v', d + ':/work:ro',
           '-e', 'BROKER_USERNAME', '-e', 'BROKER_PASSWORD',
           IMAGE, 'sh', '-c', script, env=env, check=True, stdout=subprocess.DEVNULL)
    out = docker('port', name, '8883/tcp', check=True, capture_output=True, text=True).stdout
    return int(out.split()[0].rsplit(':', 1)[1])


def wait_for_broker(name, port, ca):
    """Waits until the broker completes a TLS handshake."""
    ctx = ssl.create_default_context(cafile=ca)
    deadline = time.monotonic() + START_TIMEOUT
    while True:
        try:
            with socket.create_connection(('localhost', port), timeout=5) as s:
                with ctx.wrap_socket(s, server_hostname='localhost'):
                    return
        except OSError as e:
            running = docker('inspect', '-f', '{{.State.Running}}', name,
                             capture_output=True, text=True).stdout.strip()
            if running != 'true' or time.monotonic() > deadline:
                docker('logs', name)
                sys.exit('the broker did not start: %s' % e)
            time.sleep(0.5)


def main():
    if len(sys.argv) < 2:
        sys.exit('usage: with_broker.py <command> [<argument>...]')
    name = 'ebisu-broker-%d-%s' % (os.getpid(), secrets.token_hex(4))
    password = secrets.token_hex(16)
    with tempfile.TemporaryDirectory(prefix='ebisu-broker-') as d:
        make_certificates(d)
        shutil.copy(os.path.join(HERE, 'mosquitto.conf'), d)
        try:
            port = start_broker(d, name, password)
            wait_for_broker(name, port, os.path.join(d, 'ca.pem'))
            env = dict(os.environ,
                       EBISU_BROKER_HOST='localhost',
                       EBISU_BROKER_PORT=str(port),
                       EBISU_BROKER_CA=os.path.join(d, 'ca.pem'),
                       EBISU_BROKER_USERNAME=USERNAME,
                       EBISU_BROKER_PASSWORD=password)
            result = subprocess.run(sys.argv[1:], env=env)
            if result.returncode != 0:
                print('== the broker\'s log', flush=True)
                docker('logs', name)
            return result.returncode
        finally:
            docker('rm', '-f', name, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__':
    sys.exit(main())

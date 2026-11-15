#!/usr/bin/env python3
"""Runs a command with a Mosquitto broker of its own, for the broker tests.

The broker runs in a container, configured by mosquitto.conf, on a port
of 127.0.0.1 that the container tool picks. It speaks TLS only: its certificate,
for localhost, is issued by a CA the script makes with the openssl command, and
its one user's password is made for the run. The broker's dynamic security
plugin lets the user publish, receive and subscribe under ebisu/, except that
it refuses subscriptions under ebisu/+/refused, and lets it administer the
plugin through $CONTROL/dynamic-security, so that a test can grant those. The
command finds the broker in its environment:

    EBISU_BROKER_HOST      localhost
    EBISU_BROKER_PORT      the port
    EBISU_BROKER_CA        the CA's certificate, a PEM file
    EBISU_BROKER_USERNAME  the user
    EBISU_BROKER_PASSWORD  its password

If the command fails, the broker's log is printed. Then the container is
removed, and the temporary directory the files were made in.

    with_broker.py [--windows] <command> [<argument>...]

DOCKER names the container tool, docker by default; DOCKER_OPTS adds options to
its run, such as --security-opt label=disable for podman with SELinux.

With --windows, on Windows, the broker runs on the machine itself, from
Mosquitto's own installation, which MOSQUITTO_DIR names, C:\\Program
Files\\mosquitto by default. It listens on a port that was free, and the CA is
added to the machine's root store until the script ends, for the Schannel port
to trust. The command finds that in its environment too, so that the broker
tests leave the root store alone:

    EBISU_BROKER_CA_TRUSTED  1

That is for a machine of the run's own, and an administrator.
"""
import hashlib
import json
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
# What the user may do, for the dynamic security plugin. Its password is set in
# the container, where the plugin's tool hashes it. Anything not allowed here
# is refused, but receiving and unsubscribing: subscribing with SUBACK's 0x80,
# and publishing by dropping what the user sends.
DYNSEC = {
    'clients': [{
        'username': USERNAME,
        'roles': [{'rolename': 'tests'}, {'rolename': 'control'}],
    }],
    'roles': [{
        'rolename': 'tests',
        'acls': [
            # Before the ACLs of a lower priority, which allow it.
            {'acltype': 'subscribePattern', 'topic': 'ebisu/+/refused/#', 'priority': 10, 'allow': False},
            {'acltype': 'publishClientSend', 'topic': 'ebisu/#', 'allow': True},
            {'acltype': 'publishClientReceive', 'topic': 'ebisu/#', 'allow': True},
            {'acltype': 'subscribePattern', 'topic': 'ebisu/#', 'allow': True},
            {'acltype': 'unsubscribePattern', 'topic': 'ebisu/#', 'allow': True},
        ],
    }, {
        'rolename': 'control',
        'acls': [
            {'acltype': 'publishClientSend', 'topic': '$CONTROL/dynamic-security/#', 'allow': True},
            {'acltype': 'publishClientReceive', 'topic': '$CONTROL/dynamic-security/#', 'allow': True},
            {'acltype': 'subscribePattern', 'topic': '$CONTROL/dynamic-security/#', 'allow': True},
        ],
    }],
    'defaultACLAccess': {
        'publishClientSend': False,
        'publishClientReceive': True,
        'subscribe': False,
        'unsubscribe': True,
    },
}


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
    # made the broker's own, as the plugin writes to its file, which holds the
    # password's hash.
    script = ('mkdir /tmp/broker && cp /work/mosquitto.conf /work/dynsec.json /work/ca.pem'
              ' /work/server.pem /work/server.key /tmp/broker/'
              ' && mosquitto_ctrl -f /tmp/broker/dynsec.json dynsec setClientPassword'
              ' "$BROKER_USERNAME" "$BROKER_PASSWORD"'
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


def wait_for_broker(port, ca, running, print_log):
    """Waits until the broker completes a TLS handshake."""
    ctx = ssl.create_default_context(cafile=ca)
    deadline = time.monotonic() + START_TIMEOUT
    while True:
        try:
            with socket.create_connection(('localhost', port), timeout=5) as s:
                with ctx.wrap_socket(s, server_hostname='localhost'):
                    return
        except OSError as e:
            if not running() or time.monotonic() > deadline:
                print_log()
                sys.exit('the broker did not start: %s' % e)
            time.sleep(0.5)


def run(command, env, print_log):
    """Runs the command, and prints the broker's log if it fails."""
    result = subprocess.run(command, env=env)
    if result.returncode != 0:
        print('== the broker\'s log', flush=True)
        print_log()
    return result.returncode


def in_container(d, password, command, env):
    """Runs the command with the broker in a container."""
    name = 'ebisu-broker-%d-%s' % (os.getpid(), secrets.token_hex(4))
    shutil.copy(os.path.join(HERE, 'mosquitto.conf'), d)

    def running():
        return docker('inspect', '-f', '{{.State.Running}}', name,
                      capture_output=True, text=True).stdout.strip() == 'true'

    def print_log():
        docker('logs', name)

    try:
        port = start_broker(d, name, password)
        wait_for_broker(port, env['EBISU_BROKER_CA'], running, print_log)
        return run(command, dict(env, EBISU_BROKER_PORT=str(port)), print_log)
    finally:
        docker('rm', '-f', name, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def free_port():
    """A port nothing listens on, on 127.0.0.1, for the broker to take."""
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def windows_config(d, home, port):
    """mosquitto.conf for the broker on this machine: the plugin is the
    installation's, the files are in d, and the listener is on port, of every
    address, as Windows looks localhost up as ::1 first."""
    with open(os.path.join(HERE, 'mosquitto.conf')) as f:
        conf = f.read()
    for old, new in [
            ('plugin /usr/lib/mosquitto_dynamic_security.so\n',
             'plugin %s\n' % os.path.join(home, 'mosquitto_dynamic_security.dll')),
            ('listener 8883\n', 'listener %d\n' % port)]:
        if conf.count(old) != 1:
            sys.exit('mosquitto.conf has no line %r' % old.strip())
        conf = conf.replace(old, new)
    conf = conf.replace('/tmp/broker/', d.replace('\\', '/') + '/')
    with open(os.path.join(d, 'mosquitto.conf'), 'w') as f:
        f.write(conf)


def on_windows(d, password, command, env):
    """Runs the command with the broker on this machine, as --windows says."""
    home = os.environ.get('MOSQUITTO_DIR', r'C:\Program Files\mosquitto')
    ca = env['EBISU_BROKER_CA']
    with open(ca) as f:
        thumbprint = hashlib.sha1(ssl.PEM_cert_to_DER_cert(f.read())).hexdigest()
    log_path = os.path.join(d, 'broker.log')
    broker = None

    def print_log():
        with open(log_path, errors='replace') as f:
            print(f.read(), end='', flush=True)

    with open(log_path, 'wb') as log:
        try:
            subprocess.run(['certutil', '-addstore', '-f', 'Root', ca], check=True, stdout=subprocess.DEVNULL)
            # On mosquitto_ctrl's command line, as in the container: the machine
            # is the run's own.
            subprocess.run([os.path.join(home, 'mosquitto_ctrl.exe'), '-f', os.path.join(d, 'dynsec.json'),
                            'dynsec', 'setClientPassword', USERNAME, password],
                           check=True, stdout=subprocess.DEVNULL)
            port = free_port()
            windows_config(d, home, port)
            broker = subprocess.Popen([os.path.join(home, 'mosquitto.exe'), '-c', os.path.join(d, 'mosquitto.conf')],
                                      stdout=log, stderr=subprocess.STDOUT)
            wait_for_broker(port, ca, lambda: broker.poll() is None, print_log)
            return run(command, dict(env, EBISU_BROKER_PORT=str(port), EBISU_BROKER_CA_TRUSTED='1'), print_log)
        finally:
            # Before the directory is removed, which Windows refuses while the
            # broker has its files open.
            if broker is not None:
                broker.kill()
                broker.wait()
            subprocess.run(['certutil', '-delstore', 'Root', thumbprint], stdout=subprocess.DEVNULL)


def main():
    command = sys.argv[1:]
    windows = command[:1] == ['--windows']
    if windows:
        command = command[1:]
    if not command:
        sys.exit('usage: with_broker.py [--windows] <command> [<argument>...]')
    password = secrets.token_hex(16)
    with tempfile.TemporaryDirectory(prefix='ebisu-broker-') as d:
        make_certificates(d)
        with open(os.path.join(d, 'dynsec.json'), 'w') as f:
            json.dump(DYNSEC, f)
        env = dict(os.environ,
                   EBISU_BROKER_HOST='localhost',
                   EBISU_BROKER_CA=os.path.join(d, 'ca.pem'),
                   EBISU_BROKER_USERNAME=USERNAME,
                   EBISU_BROKER_PASSWORD=password)
        if windows:
            return on_windows(d, password, command, env)
        return in_container(d, password, command, env)


if __name__ == '__main__':
    sys.exit(main())

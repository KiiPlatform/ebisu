# The lwIP port's tests

A program for the host that tests
[`ebisu_lwip.h`](../../platform/lwip/include/ebisu_lwip.h), its
`ebisu_lwip_tcp_socket_t` and `ebisu_lwip_dns()`, on lwIP, which runs in the
program itself, on a
port of the tests' own to POSIX threads, [port](port). lwIP's sockets reach no
host from there but itself, so the servers the socket connects to are the
test's own, on lwIP's sockets too, on its loopback interface. An interface of
the test's that drops all it is given, on 10.255.0.1/24, stands for an address
that doesn't answer. lwIP is built from its release, with lwIP's own
`src/Filelists.cmake` from 2.1 on and from a list of the same files for 2.0,
whose list is for make only, and ebisu with its own CMake, which builds its
lwIP socket as lwIP's core target, `lwipcore`, is there, as an application
builds them. CTest runs the program.

- **Connections**: what is sent and received goes through; the socket connects
  again once closed; a name lwIP's DNS client resolves, from its local host
  list, and an address as written, which it takes without asking; and, in
  configuration b, IPv6.
- **Receives**: one that times out says so, and leaves the connection as it
  was; a peer's close is the end of the stream; a reset is a failure, but in
  lwIP 2.0, whose receive takes it for the end of the stream, and a send after
  it fails; so does a send to a peer that has gone.
- **`recv_wait`**: it waits for its timeout with nothing arriving, not at all
  for 0, and not for data or the end of the stream.
- **Failures to connect**: a host that doesn't resolve, with
  `lwip_getaddrinfo()`'s `EAI_FAIL`; an address that refuses, which lwIP says
  with `ECONNRESET`; and one that doesn't answer within the connect timeout,
  with `ETIMEDOUT`. Each goes to the logger, and a receive that times out
  doesn't.
- **Options**: keepalive and `TCP_NODELAY` from the next connect, and neither
  until then, as `lwip_getsockopt()` reads them, with lwIP's interval and
  count, or without `LWIP_TCP_KEEPALIVE`, a warning; the timeouts, as lwIP
  takes them.
- **Name servers**: those lwIP's DNS client asks, as `dns_setserver()` sets
  them, in order, on port 53, as many as there is room for, none without the
  first, and, in configuration b, an IPv6 one. A query is asked over a UDP
  socket of lwIP's, against a name server of the test's own on its loopback
  interface: its answer is taken, past a datagram that isn't it, but not from
  another port; one that never answers, or where nothing listens, which lwIP
  doesn't tell, is given the time given and no longer, a datagram that isn't
  the answer, just before that runs out, included; and a family lwIP is built
  without fails.
- **The DNS probe** over them: a server that answers, the next past one that
  doesn't, none that does, in the time given each, and none set; and the
  probing socket over the lwIP socket, which fails a connect in about a
  second when two servers don't answer, where lwIP's DNS client would take
  14, and connects when one does.

The program is built two ways, `EBISU_TEST_CONFIG`, as
[port/lwipopts.h](port/lwipopts.h) has them:

| | Timeouts | Waits with | `LWIP_TCP_KEEPALIVE` | IPv6 |
|---|---|---|---|---|
| `a` | `struct timeval` | select | yes | no |
| `b` | `int` milliseconds, `LWIP_SO_SNDRCVTIMEO_NONSTANDARD` | poll, but select in 2.0, which has no poll | no | yes |

against lwIP 2.0.2, the oldest the socket supports, 2.1.3 and 2.2.1, the
latest, `EBISU_TEST_LWIP`, as CI does, which runs each under valgrind too.

With CMake and a C and C++ compiler, from the repository's root:

```sh
tests/lwip/run.sh 2.0.2 a
```

which builds in `build/lwip-2.0.2-a`, and writes CTest's JUnit report in
`test-results/junit`. `tests/lwip/run.sh 2.0.2 a memcheck` runs the program
under valgrind instead, failing on an error or a definite leak.

# The FreeRTOS-Plus-TCP socket's and name servers' tests

A firmware for the MPS2 AN385 board QEMU emulates, a Cortex-M3 with a LAN9118
Ethernet controller, that tests
[`ebisu_freertos_tcp.h`](../../platform/freertos/include/ebisu_freertos_tcp.h) on
FreeRTOS-Plus-TCP: its socket, and its name servers for the DNS probe.
[run_tcp_test.py](run_tcp_test.py) starts servers on the host's loopback, TCP
ones and name servers over UDP, and runs the firmware in QEMU, whose user-mode
networking reaches them at 10.0.2.2. Their ports go to the firmware on its
command line, through semihosting. Each check that fails prints a line on the console, and
QEMU exits with 1 if any did. FreeRTOS-Kernel and FreeRTOS-Plus-TCP are built
with their own CMake and ebisu with its own, as an application builds them. The
board's support is the [FreeRTOS sample's](../../tio/freertos-sample/board/board.h).

- **A connection to an echoing server, by its address as written:** sending and
  receiving, twice, the socket used again once closed.
- **One by a name**, which `FreeRTOS_getaddrinfo()` looks up, in
  FreeRTOS-Plus-TCP's DNS cache, given the host's address, without a server.
- **A receive that times out:** it fails, `recv_timed_out` says so, and nothing
  is logged. The data that comes later is received.
- **`recv_wait`:**
  - it waits for its timeout with nothing to receive;
  - the receive timeout is as it was after it;
  - it returns at once for data received and not yet returned.
- **The end of the stream:**
  - a server that closes after its data, after which a send fails as closed,
    with nothing logged, as FreeRTOS-Plus-TCP sends nothing once the peer has
    closed its side;
  - one that resets the connection, after which a send fails as closed too,
    and the failure is logged.
- **Connecting fails:**
  - to a port nothing listens on, which QEMU's user-mode networking leaves
    unanswered rather than refuse, within the connect timeout, as a timeout;
  - for a host that doesn't resolve, as QEMU's DNS server passes the lookup to
    the host's resolver.
  - Each failure is logged.
- **An address as written isn't looked up**, as `FreeRTOS_getaddrinfo()`
  would, asking the name servers, or LLMNR, for its text: with a name server
  that never answers, connecting to an IPv4 address takes no longer than the
  connect timeout, and, with IPv6, so does connecting to an IPv6 one.
- **Asking a name server**, with the name servers' `ask`:
  - one that answers, and one that answers that the name doesn't exist, which
    is an answer too;
  - one that first sends the answer to another query, which is skipped;
  - one that sends only those, one that doesn't answer, and one that answers
    from another port: each waited for the whole of the time given, a short
    one too;
  - a port nothing listens on, whose port unreachable doesn't reach the
    socket: as one that doesn't answer;
  - many times over, each with a socket of its own;
  - an IPv6 server, without IPv6, which fails.
- **The name servers** the DNS client asks, with `servers`: QEMU's, as DHCP
  gives it, then others set on the end-points, three each: from the one the
  next lookup asks, each other in turn but those of 0, and on to a second
  end-point where the first has none for the next lookup, or after one the
  client doesn't ask, as the broadcast address.
- **The DNS probe:** QEMU's server, which passes the query on to the host's
  resolver, answers; given servers, the second answers after the first
  doesn't; and the probe's socket over the TCP one doesn't connect where no
  server answers, with `pdFREERTOS_ERRNO_ENOENT`, and connects where one does.

Built against FreeRTOS-Kernel V11.1.0 and FreeRTOS-Plus-TCP V4.2.6, those of
FreeRTOS LTS 202406, the oldest the socket supports, and V11.3.3 and V4.4.2,
the latest, as CI does, and the latest again with `ipconfigUSE_IPv6` and
`ipconfigUSE_LLMNR`, for their code to be built and the tests run with it, on
a network that is still IPv4 only.

With CMake, the Arm GNU toolchain, newlib, QEMU and Python 3 (on Ubuntu,
`cmake gcc-arm-none-eabi libnewlib-arm-none-eabi qemu-system-arm python3`),
from the repository's root:

```sh
tests/freertos_tcp/run.sh V11.1.0 V4.2.6
```

which builds in `build/freertos_tcp-V11.1.0-V4.2.6`, and writes CTest's JUnit
report in `test-results/junit`. `tests/freertos_tcp/run.sh V11.3.3 V4.4.2 ipv6`
builds with IPv6, in `build/freertos_tcp-V11.3.3-V4.4.2-ipv6`.

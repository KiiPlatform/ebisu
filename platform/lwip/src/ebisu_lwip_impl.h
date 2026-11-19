/* What the lwIP port's socket and name servers share: waiting for one of
   lwIP's sockets, with lwip_select(), or lwip_poll() where lwIP 2.1 or later is
   built without select, and why a call of its sockets failed. Private to the
   port. */
#ifndef _EBISU_LWIP_IMPL
#define _EBISU_LWIP_IMPL

#include <limits.h>
#include <stdbool.h>

#include <lwip/opt.h>
#include <lwip/init.h>
#include <lwip/errno.h>
#include <lwip/sockets.h>

/* lwIP 2.0 has select alone, whatever LWIP_SOCKET_SELECT says; 2.1 on can
   leave select or poll out. */
#if (LWIP_VERSION_MAJOR == 2 && LWIP_VERSION_MINOR == 0) || LWIP_SOCKET_SELECT
#define EBISU_LWIP_WAIT_WITH_SELECT 1
#elif LWIP_SOCKET_POLL
#define EBISU_LWIP_WAIT_WITH_SELECT 0
#else
#error "The lwIP port waits for its sockets: set LWIP_SOCKET_SELECT or LWIP_SOCKET_POLL to 1."
#endif

/* Waits up to timeout_ms, or for ever with a negative one, for the socket to
   be readable, or writable, as for a connect that finished, either way.
   Whether it was: 1, 0 for not in time, or -1 for a failure, which
   ebisu_lwip_error() says. */
static int ebisu_lwip_wait(int s, bool for_write, long timeout_ms)
{
    int ret;
#if EBISU_LWIP_WAIT_WITH_SELECT
    fd_set set;
    fd_set failed;
    struct timeval tv;
    FD_ZERO(&set);
    FD_ZERO(&failed);
    FD_SET(s, &set);
    FD_SET(s, &failed);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ret = lwip_select(s + 1, for_write ? NULL : &set, for_write ? &set : NULL, &failed,
            timeout_ms < 0 ? NULL : &tv);
#else
    struct pollfd p;
    p.fd = s;
    p.events = for_write ? POLLOUT : POLLIN;
    p.revents = 0;
    ret = lwip_poll(&p, 1, timeout_ms > INT_MAX ? INT_MAX : (int)timeout_ms);
#endif
    return ret > 0 ? 1 : ret;
}

/* lwIP 2.0 has its sockets set no errno with LWIP_SOCKET_SET_ERRNO at 0,
   which 2.1 on haven't, and keeps each socket's last error of its own all the
   same, which SO_ERROR gives, and clears. */
#if LWIP_VERSION_MAJOR == 2 && LWIP_VERSION_MINOR == 0 && defined(LWIP_SOCKET_SET_ERRNO) && \
    !LWIP_SOCKET_SET_ERRNO
#define EBISU_LWIP_ERRNO_OF_SOCKET 1
#else
#define EBISU_LWIP_ERRNO_OF_SOCKET 0
#endif

/* Why the call of lwIP's sockets just made on s failed, as an errno value:
   errno, or, where lwIP sets none, s's own error, or 0 without a socket to
   ask. Right after the call, as another call may change either. */
static int ebisu_lwip_error(int s)
{
#if EBISU_LWIP_ERRNO_OF_SOCKET
    int err = 0;
    socklen_t length = sizeof(err);
    if (s < 0 || lwip_getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &length) != 0) {
        return 0;
    }
    return err;
#else
    (void)s;
    return errno;
#endif
}

#endif /* _EBISU_LWIP_IMPL */

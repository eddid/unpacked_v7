/*
 * Copyright (c) 2015 Cesanta Software Limited
 * All rights reserved
 */

#include "v7/src/internal.h"
#include "v7/src/core.h"
#include "v7/src/string.h"
#include "v7/src/object.h"
#include "v7/src/primitive.h"
#include "v7/src/conversion.h"
#include "common/mbuf.h"
#include "common/platform.h"

#ifdef V7_ENABLE_SOCKET

#ifdef __WATCOM__
#define SOMAXCONN 128
#endif

#ifndef RECV_BUF_SIZE
#define RECV_BUF_SIZE 1024
#endif

static const char s_sock_prop[] = "__sock";

static uint32_t s_resolve(struct v7 *v7, v7_val_t ip_address) {
  size_t n;
  const char *s = v7_get_string(v7, &ip_address, &n);
  struct hostent *he = gethostbyname(s);
  return he == NULL ? 0 : *(uint32_t *) he->h_addr_list[0];
}

WARN_UNUSED_RESULT
static enum v7_err s_fd_to_sock_obj(struct v7 *v7, sock_t fd, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t sock_proto =
      v7_get(v7, v7_get(v7, v7_get_global(v7), "Socket", ~0), "prototype", ~0);

  *res = v7_mk_object(v7);
  v7_set_proto(v7, *res, sock_proto);
  v7_def(v7, *res, s_sock_prop, sizeof(s_sock_prop) - 1, V7_DESC_ENUMERABLE(0),
         v7_mk_number(v7, fd));

  return rcode;
}

/* Socket.connect(host, port [, is_udp]) -> socket_object */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Socket_connect(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);
  v7_val_t arg1 = v7_arg(v7, 1);
  v7_val_t arg2 = v7_arg(v7, 2);

  if (v7_is_number(arg1) && v7_is_string(arg0)) {
    struct sockaddr_in sin;
    sock_t sock =
        socket(AF_INET, v7_is_truthy(v7, arg2) ? SOCK_DGRAM : SOCK_STREAM, 0);
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = s_resolve(v7, arg0);
    sin.sin_port = htons((uint16_t) v7_get_double(v7, arg1));
    if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) != 0) {
      closesocket(sock);
    } else {
      rcode = s_fd_to_sock_obj(v7, sock, res);
      goto clean;
    }
  }

  *res = V7_NULL;

clean:
  return rcode;
}

/* Socket.listen(port [, ip_address [,is_udp]]) -> sock */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Socket_listen(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t arg0 = v7_arg(v7, 0);
  v7_val_t arg1 = v7_arg(v7, 1);
  v7_val_t arg2 = v7_arg(v7, 2);

  if (v7_is_number(arg0)) {
    struct sockaddr_in sin;
    int on = 1;
    sock_t sock =
        socket(AF_INET, v7_is_truthy(v7, arg2) ? SOCK_DGRAM : SOCK_STREAM, 0);
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((uint16_t) v7_get_double(v7, arg0));
    if (v7_is_string(arg1)) {
      sin.sin_addr.s_addr = s_resolve(v7, arg1);
    }

#if defined(_WIN32) && defined(SO_EXCLUSIVEADDRUSE)
    /* "Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE" http://goo.gl/RmrFTm */
    setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (void *) &on, sizeof(on));
#endif

#if !defined(_WIN32) || defined(SO_EXCLUSIVEADDRUSE)
    /*
     * SO_RESUSEADDR is not enabled on Windows because the semantics of
     * SO_REUSEADDR on UNIX and Windows is different. On Windows,
     * SO_REUSEADDR allows to bind a socket to a port without error even if
     * the port is already open by another program. This is not the behavior
     * SO_REUSEADDR was designed for, and leads to hard-to-track failure
     * scenarios. Therefore, SO_REUSEADDR was disabled on Windows unless
     * SO_EXCLUSIVEADDRUSE is supported and set on a socket.
     */
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &on, sizeof(on));
#endif

    if (bind(sock, (struct sockaddr *) &sin, sizeof(sin)) == 0) {
      listen(sock, SOMAXCONN);
      rcode = s_fd_to_sock_obj(v7, sock, res);
      goto clean;
    } else {
      closesocket(sock);
    }
  }

  *res = V7_NULL;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Socket_accept(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  v7_val_t prop = v7_get(v7, this_obj, s_sock_prop, sizeof(s_sock_prop) - 1);

  if (v7_is_number(prop)) {
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    sock_t sock = (sock_t) v7_get_double(v7, prop);
    sock_t fd = accept(sock, (struct sockaddr *) &sin, &len);
    if (fd != INVALID_SOCKET) {
      rcode = s_fd_to_sock_obj(v7, fd, res);
      if (rcode == V7_OK) {
        char *remote_host = inet_ntoa(sin.sin_addr);
        v7_set(v7, *res, "remoteHost", ~0,
               v7_mk_string(v7, remote_host, ~0, 1));
      }
      goto clean;
    }
  }
  *res = V7_NULL;

clean:
  return rcode;
}

/* sock.close() -> errno */
WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Socket_close(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  v7_val_t prop = v7_get(v7, this_obj, s_sock_prop, sizeof(s_sock_prop) - 1);
  *res = v7_mk_number(v7, closesocket((sock_t) v7_get_double(v7, prop)));

  return rcode;
}

/* sock.recv() -> string */
WARN_UNUSED_RESULT
static enum v7_err s_recv(struct v7 *v7, int all, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  v7_val_t prop = v7_get(v7, this_obj, s_sock_prop, sizeof(s_sock_prop) - 1);

  if (v7_is_number(prop)) {
    char buf[RECV_BUF_SIZE];
    sock_t sock = (sock_t) v7_get_double(v7, prop);
    struct mbuf m;
    int n;

    mbuf_init(&m, 0);
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
      mbuf_append(&m, buf, n);
      if (!all) {
        break;
      }
    }

    if (n <= 0) {
      closesocket(sock);
      v7_def(v7, this_obj, s_sock_prop, sizeof(s_sock_prop) - 1,
             V7_DESC_ENUMERABLE(0), v7_mk_number(v7, INVALID_SOCKET));
    }

    if (m.len > 0) {
      *res = v7_mk_string(v7, m.buf, m.len, 1);
      mbuf_free(&m);
      goto clean;
    }
  }

  *res = V7_NULL;

clean:
  return rcode;
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Socket_recvAll(struct v7 *v7, v7_val_t *res) {
  return s_recv(v7, 1, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Socket_recv(struct v7 *v7, v7_val_t *res) {
  return s_recv(v7, 0, res);
}

WARN_UNUSED_RESULT
V7_PRIVATE enum v7_err Socket_send(struct v7 *v7, v7_val_t *res) {
  enum v7_err rcode = V7_OK;
  v7_val_t this_obj = v7_get_this(v7);
  v7_val_t arg0 = v7_arg(v7, 0);
  v7_val_t prop = v7_get(v7, this_obj, s_sock_prop, sizeof(s_sock_prop) - 1);
  size_t len, sent = 0;

  if (v7_is_number(prop) && v7_is_string(arg0)) {
    const char *s = v7_get_string(v7, &arg0, &len);
    sock_t sock = (sock_t) v7_get_double(v7, prop);
    int n;

    while (sent < len && (n = send(sock, s + sent, len - sent, 0)) > 0) {
      sent += n;
    }
  }

  *res = v7_mk_number(v7, sent);

  return rcode;
}

void init_socket(struct v7 *v7) {
  v7_val_t socket_obj = v7_mk_object(v7), sock_proto = v7_mk_object(v7);

  v7_set(v7, v7_get_global(v7), "Socket", 6, socket_obj);
  sock_proto = v7_mk_object(v7);
  v7_set(v7, socket_obj, "prototype", 9, sock_proto);

  v7_set_method(v7, socket_obj, "connect", Socket_connect);
  v7_set_method(v7, socket_obj, "listen", Socket_listen);

  v7_set_method(v7, sock_proto, "accept", Socket_accept);
  v7_set_method(v7, sock_proto, "send", Socket_send);
  v7_set_method(v7, sock_proto, "recv", Socket_recv);
  v7_set_method(v7, sock_proto, "recvAll", Socket_recvAll);
  v7_set_method(v7, sock_proto, "close", Socket_close);

#ifdef _WIN32
  {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
    /* TODO(alashkin): add WSACleanup call */
  }
#else
  signal(SIGPIPE, SIG_IGN);
#endif
}
#else
void init_socket(struct v7 *v7) {
  (void) v7;
}
#endif

#ifndef st_err_
#define st_err_

#include "util.h"

#define _as_str(q, ...)   #q,
#define _as_const(q, ...)  q,

#define _errs                 \
  (ST_ERR),                   \
                              \
  (ST_BUG_UNINITED),          \
  (ST_AGAIN),                 \
  (ST_CHKSUM),                \
  (ST_DUP),                   \
  (ST_EXISTED),               \
  (ST_EMPTY),                 \
  (ST_EOF),                   \
  (ST_FIN),                   \
  (ST_INITTWICE),             \
  (ST_ARG_INVALID),           \
  (ST_STATE_INVALID),         \
  (ST_IO),                    \
  (ST_NOT_FOUND),             \
  (ST_NOT_READY),             \
  (ST_NOT_EQUAL),             \
  (ST_NO_GC_DATA),            \
  (ST_OUT_OF_ORDER),          \
  (ST_OUT_OF_MEMORY),         \
  (ST_OUT_OF_RANGE),          \
  (ST_TIMEOUT),               \
  (ST_UNSUPPORTED),           \
  (ST_UNINITED),              \
  (ST_BUF_NOT_ENOUGH),        \
  (ST_BUF_OVERFLOW),          \
  (ST_DISK_IN_ERR_STATE),     \
  (ST_DISK_OUT_OF_SPACE),     \
  (ST_DISK_UNAVAILABLE),      \
  (ST_INDEX_OUT_OF_RANGE),    \
  (ST_NET_AGAIN),             \
  (ST_NET_BIND),              \
  (ST_NET_CHKSUM),            \
  (ST_NET_CREATE),            \
  (ST_NET_LISTEN),            \
  (ST_NET_TIMEOUT),           \
  (ST_NET_SOCKET_OPT),        \
  (ST_NUM_OVERFLOW),          \
  (ST_RESP_DUP),              \
  (ST_RESP_LOST),             \
  (ST_RESP_TIMEOUT)

enum st_error_t {
  ST_OK = 0,
  _ST_E_START = -1001,
  // Start internal error number from -1000 in order to avoid overriding system
  // errno range(-1 to -122).
  st_foreach(_as_const, _errs)
  _ST_E_MAX
};

static const char *st_err_str_ok = "ST_OK";
static const char *st_err_str_unknown = "ST_UNKNOWN";
static const char *st_err_str_[] = {
   st_foreach(_as_str, _errs)
};

static inline const char *
st_err_str(int rc) {

  if (rc == 0) {
    return st_err_str_ok;
  }

  rc = rc - ST_ERR;

  if (rc < 0 || rc >= sizeof(st_err_str_)) {
    return st_err_str_unknown;
  }

  return st_err_str_[ rc ];
}

#undef _errs
#undef _as_str
#undef _as_const

#endif /* st_err_ */

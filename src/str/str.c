#include "str/str.h"

#include <time.h>

int
st_str_init(st_str_t *s, int64_t len) {

    st_must(s != NULL, ST_ARG_INVALID);
    st_must(len >= 0, ST_ARG_INVALID);
    st_must(s->bytes_owned == 0, ST_INITTWICE);

    int ret = ST_BUG_UNINITED;

    if (len == 0) {
        *s = (st_str_t)st_str_empty;
        return ST_OK;
    }

    uint8_t *d = (uint8_t *)st_malloc(len);

    if (d == NULL) {
        ret = ST_OUT_OF_MEMORY;
        goto err_return;
    }

    s->bytes = d;
    s->bytes_owned = 1;
    s->len = len;
    s->capacity = len;
    ret = ST_OK;

err_return:
    return ret;
}


int
st_str_init_0(st_str_t *s, int64_t len) {

    st_must(s != NULL, ST_ARG_INVALID);
    st_must(len >= 0, ST_ARG_INVALID);
    st_must(s->bytes_owned == 0, ST_INITTWICE);

    int ret = st_str_init(s, len + 1);

    if (ret != ST_OK) {
        return ret;
    }

    s->len = len;

    return ST_OK;
}


int
st_str_destroy(st_str_t *s) {
    st_must(s != NULL, ST_ARG_INVALID);

    if (s->bytes_owned) {
        st_free(s->bytes);
    }

    s->type = -1;
    s->bytes = NULL;
    s->bytes_owned = 0;
    s->len = 0;
    s->capacity = 0;

    return ST_OK;
}

int
st_str_ref(st_str_t *s, const st_str_t *target) {
    st_must(s != NULL, ST_ARG_INVALID);
    st_must(s->bytes_owned == 0, ST_INITTWICE);
    st_must(s != target, ST_ARG_INVALID);
    st_must(target != NULL, ST_ARG_INVALID);

    int ret = ST_OK;

    *s = *target;
    s->bytes_owned = 0;

    return ret;
}

/*
 * st_str_t *st_str_pdup(STMemPool *pool, const st_str_t *s) {
 *   st_must_be(pool != NULL && s != NULL && s->inited_, NULL);
 *   st_str_t *ret = NULL;
 *   if ((ret = (st_str_t *)st_mem_pool_alloc(pool, sizeof(*s))) == NULL) {
 *     //nothing
 *   } else if ((ret->bytes = (char *)st_mem_pool_alloc(pool, s->len + s->trailing_0)) == NULL) {
 *     ret = NULL;
 *   } else {
 *     memcpy(ret->bytes, s->bytes, s->len);
 *     ret->bytes_owned = 0; //do not own this bytes.free by others
 *     ret->len = s->len;
 *     ret->inited_ = 1;
 *     ret->trailing_0 = s->trailing_0;
 *   }
 *   return ret;
 * }
 */


int
st_str_copy(st_str_t *str, const st_str_t *from) {
    st_must(str != NULL, ST_ARG_INVALID);
    st_must(str->bytes_owned == 0, ST_INITTWICE);
    st_must(from != NULL, ST_ARG_INVALID);
    st_must(str != from, ST_ARG_INVALID);

    int ret;

    if (from->capacity == 0) {

        if (from->bytes_owned == 1) {
            derr("zero bytes st_str_t should not own any data");
            return ST_ARG_INVALID;
        }

        *str = *from;
        return ST_OK;
    }

    ret = st_str_init(str, from->capacity);
    if (ret != ST_OK) {
        return ret;
    }

    st_memcpy(str->bytes, from->bytes, from->capacity);
    str->len = from->len;
    str->type = from->type;

    return ST_OK;
}

int
st_str_copy_cstr(st_str_t *str, const char *s) {

    st_must(str != NULL, ST_ARG_INVALID);
    st_must(str->bytes_owned == 0, ST_INITTWICE);
    st_must(s != NULL, ST_ARG_INVALID);

    int slen;
    int ret;

    slen = strlen(s);
    if (slen == 0) {
        *str = (st_str_t)st_str_empty;
        return ST_OK;
    }

    ret = st_str_init_0(str, slen);
    if (ret != ST_OK) {
        return ret;
    }

    st_memcpy(str->bytes, s, slen + 1);
    str->type = 0;

    return ST_OK;
}

int
st_str_seize(st_str_t *str, st_str_t *from) {

    st_must(str != NULL, ST_ARG_INVALID);
    st_must(str->bytes_owned == 0, ST_INITTWICE);
    st_must(from != NULL, ST_ARG_INVALID);
    st_must(str != from, ST_ARG_INVALID);

    *str = *from;
    from->bytes_owned = 0;

    return ST_OK;
}

/*
 * int st_str_to_str(const st_str_t *ptr, char *buf, const int64_t len, int64_t *pos) {
 *   st_must_be(ptr && ptr->inited_ && buf && len > 0 && pos, ST_ERR_INVALID_ARG);
 *   st_must_be(0 <= *pos && *pos < len && ptr->len <= len - *pos - 1, ST_ERR_BUF_OVERFLOW);
 *   memcpy(buf + *pos, ptr->bytes, ptr->len);
 *   buf[*pos + ptr->len] = '\0';
 *   *pos += ptr->len;
 *   return ST_OK;
 * }
 */

int
st_str_cmp(const st_str_t *a, const st_str_t *b) {

    st_must(a != NULL, ST_ARG_INVALID);
    st_must(b != NULL, ST_ARG_INVALID);

    if (a->type - b->type != 0) {
        return a->type - b->type > 0 ? 1 : -1;
    }

    if (a->bytes == NULL) {
        if (b->bytes == NULL) {
            return 0;
        } else {
            return -1;
        }
    }

    /* a->bytes != NULL */

    if (b->bytes == NULL) {
        return 1;
    }

    int64_t l = st_min(a->len, b->len);

    int ret = st_memcmp(a->bytes, b->bytes, l);
    if (ret != 0) {
        return ret > 0 ? 1 : -1;
    }

    return st_cmp(a->len, b->len);
}

/*
 * int st_str_join_with_n(st_str_t *s, st_str_t *sep, int n, st_str_t **elts) {
 *
 *   st_must_be(s != NULL, ST_ERR_INVALID_ARG);
 *   st_must_be(s->inited_ == 0, ST_ERR_INITTWICE);
 *
 *   st_must_be(n > 0, ST_ERR_INVALID_ARG);
 *
 *   st_must_be(sep != NULL, ST_ERR_INVALID_ARG);
 *   st_must_be(sep->inited_ == 1, ST_ERR_UNINITED);
 *
 *   int ret = ST_FAIL;
 *   int64_t size = 0;
 *   st_str_t tmp = st_str_null;
 *   const st_str_t *elt = NULL;
 *   char *p = NULL;
 *
 *   for (int i = 0; i < n; i++) {
 *     if (elts[i] == NULL || elts[i]->inited_ == 0) {
 *       return ST_ERR_INVALID_ARG;
 *     }
 *     size += sep->len + elts[i]->len;
 *   }
 *
 *   size -= sep->len;
 *
 *
 *   ret = st_str_init_0(&tmp, size);
 *   st_check_ret(ret, "init str with size: %d", (int)size);
 *
 *   p = tmp.bytes;
 *   for (int i = 0; i < n; i++) {
 *
 *     elt = elts[i];
 *
 *     st_memcpy(p, elt->bytes, elt->len);
 *     p += elt->len;
 *
 *     if (i < n - 1) {
 *       st_memcpy(p, sep->bytes, sep->len);
 *       p += sep->len;
 *     }
 *   }
 *
 *   *s = tmp;
 *
 * exit:
 *
 *   return ret;
 * }
 */

/*
 * int64_t st_str_size(void *val) {
 *   //4 is st_str_t len element size
 *   return ((st_str_t *)val)->len + 4;
 * }
 *
 * int st_str_serialize(void *val, struct STBuf *buf) {
 *   st_str_t *str = val;
 *
 *   st_must_be(str && buf, ST_ERR_INVALID_ARG);
 *   st_must_be(str->inited_, ST_ERR_INVALID_ARG);
 *
 *   struct STBuf b = *buf;
 *
 *   int ret = ST_FAIL;
 *   uint32_t len = str->len;
 *
 *   //note str length just use 4 bytes, compatible with rpc length
 *   st_check_ret(ret = st_u32_serialize(&len, &b), "");
 *
 *   st_check_ret(ret = st_encode_data(&b, str->bytes, str->len), "");
 *
 *   *buf = b;
 *
 * exit:
 *   return ret;
 * }
 *
 * int st_str_deserialize(void *val, struct STBuf *buf) {
 *
 *   st_str_t *str = val;
 *
 *   st_must_be(str, ST_ERR_INVALID_ARG);
 *   st_must_be(str->inited_ == 0, ST_ERR_INVALID_ARG);
 *   st_must_be(buf, ST_ERR_INVALID_ARG);
 *
 *   struct STBuf b = *buf;
 *   int ret = ST_FAIL;
 *   uint32_t len = 0;
 *
 *   //note str length just use 4 bytes, compatible with rpc length
 *   st_check_ret(ret = st_u32_deserialize(&len, &b), "deserialize str.len: buf: %s", st_buf_to_cstr(&b));
 *
 *   st_check_ret(ret = st_str_init(str, len), "init str: len=%llu", (long long unsigned)len);
 *
 *   st_check_ret(ret = st_decode_data(&b, str->bytes, str->len), "buf: %s", st_buf_to_cstr(&b));
 *
 *   *buf = b;
 *
 * exit:
 *   if (ST_OK != ret) {
 *     st_str_destroy(str);
 *   }
 *   return ret;
 * }
 */

/*
 * int st_str_to_int(const st_str_t* str, int64_t* int_val) {
 *   st_must_be(str != NULL, ST_ERR_INVALID_ARG);
 *   st_must_be(str->inited_ == 1, ST_ERR_INVALID_ARG);
 *   st_must_be(int_val != NULL, ST_ERR_INVALID_ARG);
 *   int ret = ST_OK;
 *   char *ptr = NULL;
 *   const char *bytes = st_str_to_cstr(str);
 *   if (*bytes == '\0') {
 *     ret = ST_ERR_CAST_FAIL;
 *   } else {
 *     int64_t ret_val = strtol(bytes, &ptr, 10);
 *     if (ptr != bytes + str->len) {
 *       ret = ST_ERR_CAST_FAIL;
 *     } else {
 *       *int_val = ret_val;
 *     }
 *   }
 *   return ret;
 * }
 *
 *
 * int st_str_to_double(const st_str_t* str, double* double_val) {
 *   st_must_be(str != NULL, ST_ERR_INVALID_ARG);
 *   st_must_be(str->inited_ == 1, ST_ERR_INVALID_ARG);
 *   st_must_be(double_val != NULL, ST_ERR_INVALID_ARG);
 *   int ret = ST_OK;
 *   char *ptr = NULL;
 *   const char *bytes = st_str_to_cstr(str);
 *   if (*bytes == '\0') {
 *     ret = ST_ERR_CAST_FAIL;
 *   } else {
 *     double ret_val = strtod(bytes, &ptr);
 *     if (ptr != bytes + str->len) {
 *       ret = ST_ERR_CAST_FAIL;
 *     } else {
 *       *double_val = ret_val;
 *     }
 *   }
 *   return ret;
 * }
 *
 * int st_str_to_float(const st_str_t* str, float* float_val) {
 *   st_must_be(str != NULL, ST_ERR_INVALID_ARG);
 *   st_must_be(str->inited_ == 1, ST_ERR_INVALID_ARG);
 *   st_must_be(float_val != NULL, ST_ERR_INVALID_ARG);
 *   int ret = ST_OK;
 *   char *ptr = NULL;
 *   const char *bytes = st_str_to_cstr(str);
 *   if (*bytes == '\0') {
 *     ret = ST_ERR_CAST_FAIL;
 *   } else {
 *     float ret_val = strtof(bytes, &ptr);
 *     if (ptr != bytes + str->len) {
 *       ret = ST_ERR_CAST_FAIL;
 *     } else {
 *       *float_val = ret_val;
 *     }
 *   }
 *   return ret;
 * }
 *
 * int st_str_to_bool(const struct st_str_t* str, int32_t* bool_val) {
 *   int64_t int_val;
 *   int ret = st_str_to_int(str, &int_val);
 *   if (ret == ST_OK) {
 *     if (int_val == 1 || int_val == 0) {
 *       *bool_val = (int32_t)int_val;
 *     } else {
 *       ret = ST_ERR_CAST_FAIL;
 *     }
 *   }
 *   return ret;
 * }
 * int st_str_to_time(const struct st_str_t* str, time_t* time_val) {
 *   st_must_be(str != NULL, ST_ERR_INVALID_ARG);
 *   st_must_be(str->inited_ == 1, ST_ERR_INVALID_ARG);
 *   st_must_be(time_val != NULL, ST_ERR_INVALID_ARG);
 *   int ret = ST_OK;
 *   const char *bytes = st_str_to_cstr(str);
 *   struct tm datetime_val;
 *   memset(&datetime_val, 0, sizeof(struct tm));
 *   if (strptime(bytes, "%Y-%m-%d %H:%M:%S", &datetime_val) != NULL) {
 *     *time_val = mktime(&datetime_val);
 *   } else {
 *     ret = ST_ERR_CAST_FAIL;
 *   }
 *   return ret;
 * }
 */

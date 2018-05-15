#ifndef __ST_VERSION_H_INCLUDE__
#define __ST_VERSION_H_INCLUDE__


#define ST_VERSION_LEN_MAX 32

typedef enum st_version_relation_e {
    ST_VERSION_COMPATIBLE = 0,
    ST_VERSION_CONFLICT   = 1,
    ST_VERSION_THE_SAME   = 2,
} st_version_relation_t;


int st_version_is_compatible(const char *v1, const char *v2);
const char *st_version_get_fully(void);

#endif

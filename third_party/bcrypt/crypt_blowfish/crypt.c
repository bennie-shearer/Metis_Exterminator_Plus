/*
 * Wrapper providing ow-crypt.h API (crypt_rn, crypt_gensalt_rn, etc.)
 * on top of the Solar Designer _crypt_blowfish_rn / _crypt_gensalt_blowfish_rn
 * internal API.
 */
#include <stdlib.h>
#include "ow-crypt.h"
#include "crypt_blowfish.h"
#include "crypt_gensalt.h"

char *crypt_rn(const char *key, const char *setting, void *data, int size) {
    return _crypt_blowfish_rn(key, setting, (char *)data, size);
}

char *crypt_ra(const char *key, const char *setting, void **data, int *size) {
    if (!*data) { *size = 128; *data = malloc(*size); if (!*data) return NULL; }
    return _crypt_blowfish_rn(key, setting, (char *)*data, *size);
}

char *crypt_r(const char *key, const char *setting, void *data) {
    return _crypt_blowfish_rn(key, setting, (char *)data, 128);
}

char *crypt(const char *key, const char *setting) {
    static char buf[128];
    return _crypt_blowfish_rn(key, setting, buf, sizeof(buf));
}

char *crypt_gensalt_rn(const char *prefix, unsigned long count,
    const char *input, int size, char *output, int output_size) {
    return _crypt_gensalt_blowfish_rn(prefix, count, input, size, output, output_size);
}

char *crypt_gensalt_ra(const char *prefix, unsigned long count,
    const char *input, int size) {
    char *buf = malloc(30);
    if (!buf) return NULL;
    if (!_crypt_gensalt_blowfish_rn(prefix, count, input, size, buf, 30)) {
        free(buf); return NULL;
    }
    return buf;
}

char *crypt_gensalt_r(const char *prefix, unsigned long count,
    const char *input, int size, char *output, int output_size) {
    return _crypt_gensalt_blowfish_rn(prefix, count, input, size, output, output_size);
}

char *crypt_gensalt(const char *prefix, unsigned long count,
    const char *input, int size) {
    static char buf[30];
    return _crypt_gensalt_blowfish_rn(prefix, count, input, size, buf, sizeof(buf));
}

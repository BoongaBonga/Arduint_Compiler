#ifndef ARDUINT_COMMON_H
#define ARDUINT_COMMON_H

#include <stdint.h>

#define POPULATE_ARRAY(type, arr, ...)                                         \
  do {                                                                         \
    type _tmp[] = {__VA_ARGS__};                                               \
    size_t _src_len = sizeof(_tmp) / sizeof(type);                             \
    size_t _dst_len = sizeof(arr) / sizeof(type);                              \
    size_t _n = (_src_len < _dst_len) ? _src_len : _dst_len;                   \
    memcpy((arr), _tmp, _n * sizeof(type));                                    \
  } while (0)

#define UINT8_COUNT UINT8_MAX + 1

// #define STATUS_PRINTING

#endif
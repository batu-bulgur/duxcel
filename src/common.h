#ifndef DUXCEL_COMMON_H
#define DUXCEL_COMMON_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DX_SHEET_ROWS UINT64_C(1099511627776)
#define DX_CANVAS_MIN_ROWS UINT64_C(256)

/* All dx_* strings belong to the caller and use free(). Allocation failure is fatal. */
#if defined(__clang__) || defined(__GNUC__)
#define DX_NONNULL_RESULT __attribute__((returns_nonnull))
#else
#define DX_NONNULL_RESULT
#endif
void *dx_alloc(size_t size) DX_NONNULL_RESULT;
void *dx_realloc(void *ptr, size_t size) DX_NONNULL_RESULT;
char *dx_strdup(const char *text) DX_NONNULL_RESULT;
char *dx_printf(const char *format, ...) DX_NONNULL_RESULT;
void dx_append(char **text, const char *format, ...);
char *dx_quote(const char *text, char quote);
const char *dx_basename(const char *path);
size_t dx_utf8_prev(const char *text, size_t length);
#endif

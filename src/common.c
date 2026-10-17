#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *dx_alloc(size_t size) {
    void *p = calloc(1, size ? size : 1);
    if (!p) { fputs("Duxcel: out of memory\n", stderr); abort(); }
    return p;
}
void *dx_realloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size ? size : 1);
    if (!p) { fputs("Duxcel: out of memory\n", stderr); abort(); }
    return p;
}
char *dx_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = dx_alloc(n); memcpy(p, s, n); return p;
}
static char *vformat(const char *fmt, va_list args) {
    va_list copy; va_copy(copy, args);
    int n = vsnprintf(NULL, 0, fmt, copy); va_end(copy);
    if (n < 0) return dx_strdup("Formatting error");
    char *s = dx_alloc((size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, args); return s;
}
char *dx_printf(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    char *s = vformat(fmt, args); va_end(args); return s;
}
void dx_append(char **s, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    char *tail = vformat(fmt, args); va_end(args);
    size_t a = *s ? strlen(*s) : 0, b = strlen(tail);
    *s = dx_realloc(*s, a + b + 1); memcpy(*s + a, tail, b + 1); free(tail);
}
char *dx_quote(const char *s, char q) {
    size_t n = 3;
    for (const char *p = s; *p; p++) n += *p == q ? 2 : 1;
    char *out = dx_alloc(n), *p = out; *p++ = q;
    while (*s) { if (*s == q) *p++ = q; *p++ = *s++; }
    *p++ = q; *p = 0; return out;
}
const char *dx_basename(const char *path) {
    const char *base = path;
    for (; *path; path++) if (*path == '/' || *path == '\\') base = path + 1;
    return base;
}
size_t dx_utf8_prev(const char *s, size_t n) {
    if (!n) return 0;
    n--; while (n && ((unsigned char)s[n] & 0xc0) == 0x80) n--;
    return n;
}

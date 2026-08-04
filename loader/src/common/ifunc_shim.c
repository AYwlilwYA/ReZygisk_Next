/* INFO: Tango, a binary translator, is used on aarch64-only devices to run 32-bit ARM binaries.
         During our own injection into the 32-bit app_process, we can't (yet?) resolve IFUNC
           symbols from the 64-bit ptrace host, as it requires executing the ARM32 IFUNC resolver code.

         These shims allow the linker to resolve these symbols locally, without importing them from libc.so,
           so the remote CSOLoader doesn't have to handle IFUNC resolution for these symbols.

         _chk variants are also shimmed to avoid importing those from libc, since _FORTIFY_SOURCE
           rewrites calls to them. */

/* INFO: Only needed for arm32, which is for Tango Zygote/ReZygisk */
#ifdef __arm__

/* Prevent _FORTIFY_SOURCE from rewriting our definitions */
#undef _FORTIFY_SOURCE

#include <stddef.h>

__attribute__((visibility("hidden"), used))
void *memcpy(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  for (size_t i = 0; i < n; i++)
    d[i] = s[i];

  return dst;
}

__attribute__((visibility("hidden"), used))
void *memmove(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  if (d < s) {
    for (size_t i = 0; i < n; i++)
      d[i] = s[i];
  } else if (d > s) {
    for (size_t i = n; i > 0; i--)
      d[i - 1] = s[i - 1];
  }

  return dst;
}

__attribute__((visibility("hidden"), used))
char *strcpy(char *dst, const char *src) {
  char *d = dst;
  while ((*d++ = *src++))
    ;

  return dst;
}

/* INFO: _FORTIFY_SOURCE variants */
__attribute__((visibility("hidden"), used))
char *__strcpy_chk(char *dst, const char *src, size_t dst_len) {
  (void)dst_len; /* skip fortify check — we're a minimal shim */
  char *d = dst;
  while ((*d++ = *src++))
    ;

  return dst;
}

__attribute__((visibility("hidden"), used))
void *__memset_chk(void *dst, int c, size_t n, size_t dst_len) {
  (void)dst_len;
  unsigned char *d = (unsigned char *)dst;
  for (size_t i = 0; i < n; i++)
    d[i] = (unsigned char)c;

  return dst;
}

__attribute__((visibility("hidden"), used))
int strcmp(const char *s1, const char *s2) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;
  while (*p1 && *p1 == *p2) {
    p1++;
    p2++;
  }

  return (int)*p1 - (int)*p2;
}

__attribute__((visibility("hidden"), used))
int strncmp(const char *s1, const char *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;
  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i])
      return (int)p1[i] - (int)p2[i];

    if (p1[i] == '\0')
      return 0;
  }

  return 0;
}

__attribute__((visibility("hidden"), used))
int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;
  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i])
      return (int)p1[i] - (int)p2[i];
  }

  return 0;
}

__attribute__((visibility("hidden"), used))
char *strstr(const char *haystack, const char *needle) {
  if (!*needle)
    return (char *)haystack;

  for (; *haystack; haystack++) {
    const char *h = haystack;
    const char *n = needle;
    while (*h && *n && *h == *n) {
      h++;
      n++;
    }

    if (!*n)
      return (char *)haystack;
  }

  return (void *)0;
}

#endif /* __arm__ */

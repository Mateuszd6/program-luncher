#ifndef __UTIL_H__
#define __UTIL_H__

static inline int GetLettersCount(const char *text);

static inline char ToLower(const char c);

static inline int PrefixMatch(const char *text, const char *pattern);

static inline int SuffixMatch(const char *text, const char *pattern);

static char *DuplicateString(const char *s);

#endif // __UTIL_H__

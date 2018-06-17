#ifndef __UTIL_H__
#define __UTIL_H__

#ifndef MAX
#define MAX(x,y) (                                      \
        { __auto_type __x = (x); __auto_type __y = (y); \
            __x > __y ? __x : __y; })
#endif

static inline int GetLettersCount(const char *text);

static inline char ToLower(const char c);

static inline int PrefixMatch(const char *text, const char *pattern);

static inline int PrefixMatchOfLengthN(const char *text,
                                       const char *pattern,
                                       const int length);

static inline int PrefixMatchWithTrimmedPattern(const char *text, const char *pattern);

static inline int SuffixMatch(const char *text, const char *pattern);

static inline int TextMatch(const char *text, const char *pattern);

static char *DuplicateString(const char *s);

#endif // __UTIL_H__

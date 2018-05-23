#ifndef __UTIL_H__
#define __UTIL_H__

#ifndef MAX
#define MAX(x,y) (                                      \
        { __auto_type __x = (x); __auto_type __y = (y); \
            __x > __y ? __x : __y; })
#endif

#ifdef DEBUG
static clock_t CURR_PERFORMACE_CNT;
#define PQUERY_START_TIMER(MSG)                 \
    do                                          \
    {                                           \
        fprintf(stderr, "%s\n", MSG);           \
        CURR_PERFORMACE_CNT = clock(); }        \
    while(0);

#define PQUERY_STOP_TIMER(MSG)                                          \
    do                                                                  \
    {                                                                   \
        CURR_PERFORMACE_CNT = clock() - CURR_PERFORMACE_CNT;            \
        fprintf(stderr, "%s: %f\n", (MSG),                              \
                ((double)CURR_PERFORMACE_CNT)/CLOCKS_PER_SEC);          \
    } while(0);
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

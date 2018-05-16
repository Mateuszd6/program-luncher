#include "util.h"

// [text] must be \0 terminated.
static inline int GetLettersCount(const char *text)
{
    for (int i = 0;; ++i)
        if (text[i] == '\0')
            return i;
}

static inline char ToLower(const char c)
{
    if ('A' <= c && c <= 'Z')
        return c + 'a' - 'A';

    return c;
}

// Is [pattern] a prefix of [text]?
static inline int PrefixMatch(const char *text, const char *pattern)
{
#if 0
    printf("\tDoin prefix match: %s, %s\n", text, pattern);
#endif

    for (int i = 0;; ++i)
    {
        if (pattern[i] == '\0')
            return 1;

        if (text[i] == '\0' ||
            (case_sensitive ? text[i] != pattern[i]
                            : ToLower(text[i]) != ToLower(pattern[i])))
            return 0;
    }
}

// Is [pattern] a prefix of [text]?
static inline int PrefixMatchWithTrimmedPattern(const char *text,
                                                const char *pattern)
{
#if 0
    printf("\tDoin prefix match: %s, %s\n", text, pattern);
#endif
    while (*pattern == ' ')
        pattern++;

    const char *end = pattern;
    while (*end != '\0')
        end++;

    while (end != pattern && *(end - 1) == ' ')
        end--;

    for (int i = 0;; ++i)
    {
        if (&pattern[i] == end)
            return 1;

        if (text[i] == '\0' ||
            (case_sensitive ? text[i] != pattern[i]
                            : ToLower(text[i]) != ToLower(pattern[i])))
            return 0;
    }
}

// Is [pattern] a suffix of [text]?
static inline int SuffixMatch(const char *text, const char *pattern)
{
    int text_len = 0;
    int pattern_len = 0;

    while (text[text_len] != '\0')
        text_len++;
    while (pattern[pattern_len] != '\0')
        pattern_len++;

    if (text_len < pattern_len)
        return 0;

    for (int i = 0; i < pattern_len; ++i)
    {
        assert(text[text_len - pattern_len + i] != '\0');
        if ((case_sensitive ? text[text_len - pattern_len + i] != pattern[i]
                            : ToLower(text[text_len - pattern_len + i]) !=
                                  ToLower(pattern[i])))
        {
            return 0;
        }
    }

    return 1;
}

// TODO: implement KMP.
static inline int PatternMatchBruteForce(const char *text, const char *pattern)
{
    for (int i = 0; text[i] != '\0'; ++i)
        if (PrefixMatch(&text[i], pattern))
            return 1;

    return 0;
}

static inline int TextMatch(const char *text, const char *pattern)
{
    char buffer[strlen(pattern) + 1];
    const char *previous = pattern;
    const char *current;
    for (current = pattern;; current++)
    {
        if ((*current) == ' ' || (*current) == '\0')
        {
            size_t len = current - previous;
            strncpy(buffer, previous, len);
            buffer[len] = '\0';
            int match = PatternMatchBruteForce(text, buffer);

#if 0
            printf("[%s]\tPATTERN: %s(len: %ld), TEXT: %s\n",
                   match ? "x" : " ", buffer, len, text);
#endif

            if (!match)
                return 0;

            if ((*current) == '\0')
                break;

            previous = current + 1;
        }
    }

    return 1;
}

static char *DuplicateString(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    if (p)
    {
        strcpy(p, s);
    }
    return p;
}

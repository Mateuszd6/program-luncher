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
    for (int i = 0;; ++i)
    {
        if (pattern[i] == '\0')
            return 1;

        if (text[i] == '\0' || (case_sensitive ? text[i] != pattern[i] :
                                ToLower(text[i]) != ToLower(pattern[i])))
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
        assert(text[text_len-pattern_len+i] != '\0');
        if ((case_sensitive ? text[text_len-pattern_len+i] != pattern[i] :
             ToLower(text[text_len-pattern_len+i]) != ToLower(pattern[i])))
            return 0;
    }

    return 1;
}


static char *DuplicateString(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    if(p) { strcpy(p, s); }
    return p;
}

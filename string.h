#ifndef __STRING_H__
#define __STRING_H__

// If the entry is less than this characters long, it will be stored in a
// struct, not by the pointer, and the access will be faster.
#ifndef STRING_CHARACTERS_IN_SHORT_TEXT
#define STRING_CHARACTERS_IN_SHORT_TEXT (56)
#endif

typedef struct
{
    char text[STRING_CHARACTERS_IN_SHORT_TEXT];
    char *large_text;
} String;

static inline char *StringGetText(String *str);

static inline String StringMakeEmpty();

static inline String MakeStringCopyText(const char *text, int text_len);

static inline int StringIsEmpty(const String *str);

static inline int StringCompare(String *self, String *other);

#endif // __STRING_H__

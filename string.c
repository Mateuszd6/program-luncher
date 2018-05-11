#include "string.h"

static inline char *StringGetText(String *str)
{
    return str->large_text ? str->large_text : str->text;
}

static inline String StringMakeEmpty()
{
    String result;
    result.text[0] = '\0';
    result.large_text = NULL;

    return result;
}

static inline String MakeStringCopyText(const char *text, int text_len)
{
    String result;
    if (text_len >= STRING_CHARACTERS_IN_SHORT_TEXT)
    {
        result.large_text = malloc(sizeof(char) * (text_len + 1));
        memcpy(result.large_text, text, sizeof(char) * text_len);
        result.large_text[text_len] = '\0';
    }
    else
    {
        result.large_text = NULL;
        memcpy(result.text, text, sizeof(char) * text_len);
        result.text[text_len] = '\0';
    }

    return result;
}

static inline int StringIsEmpty(const String *str)
{
    return (!str->large_text && str->text[0] == '\0');
}

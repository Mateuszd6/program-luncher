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

static inline char *StringGetText(String *str)
{
    return str->large_text ? str->large_text : str->text;
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

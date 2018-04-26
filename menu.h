#ifndef MENU_H
#define MENU_H

char inserted_text[256];
int inserted_text_idx;

// If the entry is less than this characters long, it will be stored in a
// struct, not by the pointer, and the access will be faster.
#define CHARACTERS_IN_SHORT_TEXT (48)

struct _Entry
{
    char text[CHARACTERS_IN_SHORT_TEXT];
    char *large_text;

    struct _Entry *next_displayed;
    struct _Entry *prev_displayed;
};
typedef struct _Entry Entry;

#define NUMBER_OF_ENTRIES_IN_BLOCK (256)
struct _EntriesBlock
{
    Entry entries[NUMBER_OF_ENTRIES_IN_BLOCK];
    int number_of_entries;

    struct _EntriesBlock *next;
};
typedef struct _EntriesBlock EntriesBlock;

extern Entry *first_displayed_entry;
extern EntriesBlock first_entries_block;
extern Entry *current_select;
extern int current_select_idx;
extern int current_select_offset;

extern int GetLettersCount(const char *text);

extern char *GetText(Entry *entry);

void CompleteAtCurrent();

void UpdateDisplayedEntries();

void AddEntry(char *value, int length);

#endif

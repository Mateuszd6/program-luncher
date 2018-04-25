#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "menu.h"
#include "assert.h"

char inserted_tettxt[256];
int inserted_text_idx = 0;

// The first block which will be usually enought for all entries.
// If there are dozens of them, next blocks will be allocated.
EntriesBlock first_entries_block = {};
static EntriesBlock *last_block = &first_entries_block;

Entry *first_displayed_entry;

// A pointer to the selected entry, must be in a printed list!
Entry *current_select;

// Which rect (from the top) is highlighted?
int current_select_idx = 0;

// How many rects (from the top) we must skip to see selected one?
int current_select_offset = 0;

static int case_sensitive = 0;

static char ToLower(const char c)
{
    if ('A' <= c && c <= 'Z')
        return c + 'a' - 'A';

    return c;
}

static int PrefixMatch(const char *text, const char *pattern)
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

// [text] must be \0 terminated.
int GetLettersCount(const char *text)
{
    for (int i = 0;; ++i)
        if (text[i] == '\0')
            return i;
}

inline static int EntryMatch(const Entry *entry, const char *text)
{
    if (entry->large_text)
        return PrefixMatch(entry->large_text, text);
    else
        return PrefixMatch(entry->text, text);
}

inline char *GetText(Entry *entry)
{
    return entry->large_text
        ? entry->large_text
        : &(entry->text[0]);
}

void CompleteAtCurrent()
{
    if (current_select)
    {
        char *current_complete = GetText(current_select);
        int curr_complete_size = GetLettersCount(current_complete);
        for (int i = 0; i < curr_complete_size; ++i)
            inserted_text[i] = current_complete[i];
        inserted_text_idx = curr_complete_size;
    }
    else
    {
        assert(first_displayed_entry);
        fprintf(stderr, "No option to complete. TAB ignored...\n");
    }
}

void UpdateDisplayedEntries()
{
    first_displayed_entry = NULL;

    current_select = NULL;

    Entry *prev_displayed_entry = NULL;
    EntriesBlock *current_block = &first_entries_block;

    do
    {
        for (int i = 0; i < current_block->number_of_entries; ++i)
        {
            Entry *curr_entry = &(current_block->entries[i]);

            if (inserted_text[0] == '\0'
                || EntryMatch(curr_entry, inserted_text))
            {
                if (prev_displayed_entry)
                {
                    // We append to the list.
                    prev_displayed_entry->next_displayed = curr_entry;
                    curr_entry->prev_displayed = prev_displayed_entry;
                }
                else
                {
                    // This is a first displayed entry.
                    first_displayed_entry = curr_entry;
                    curr_entry->prev_displayed = NULL;
                }

                curr_entry->next_displayed = NULL;
                prev_displayed_entry = curr_entry;
            }
        }
        current_block = current_block->next;
    }
    while (current_block);

    current_select = first_displayed_entry;
    current_select_idx = 0;
    current_select_offset = 0;
}

// [value] must be copied!
void AddEntry(char *value, int length)
{
    if (last_block->number_of_entries == NUMBER_OF_ENTRIES_IN_BLOCK)
    {
        // We must allocate new new_entry block.
        EntriesBlock *block = malloc(sizeof(EntriesBlock));
        block->next = NULL;
        block->number_of_entries = 0;

        last_block->next = block;
        last_block = block;
    }

    Entry *new_entry = &(last_block->entries[last_block->number_of_entries++]);
    if (length >= CHARACTERS_IN_SHORT_TEXT)
    {
        // Assumes that this will work...
        // TODO: Handle the error case!
        new_entry->large_text = strcpy(malloc(sizeof(char) * length), value);
    }
    else
    {
        new_entry->large_text = NULL;
        memcpy(new_entry->text, value, sizeof(char) * length);
        new_entry->text[length] = '\0';
    }
    new_entry->next_displayed = NULL;
}

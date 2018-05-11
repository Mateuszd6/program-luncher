#include "string.h"

struct _Entry
{
    String text;

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

// TODO: Move to some menu state strucutre!
// Used only when reading .desktop files.
DesktopEntry *desktop_entries;
int desktop_entries_size = 0;

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

char inserted_text[256];
int inserted_text_idx;

inline static int EntryMatch(Entry *entry, const char *text)
{
    return PrefixMatch(StringGetText(&entry->text), text);
}

static void AddEntry(char *value, int length)
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

    new_entry->text = MakeStringCopyText(value, length);
    new_entry->next_displayed = NULL;
}

static void CompleteAtCurrent()
{
    if (current_select)
    {
        char *current_complete = StringGetText(&current_select->text);
        int curr_complete_size = GetLettersCount(current_complete);
        for (int i = 0; i < curr_complete_size; ++i)
            inserted_text[i] = current_complete[i];
        inserted_text_idx = curr_complete_size;
    }
    else
    {
        // TODO: Make sure this assert is correct!
        assert(!first_displayed_entry);
        fprintf(stderr, "No option to complete. TAB ignored...\n");
    }
}

static void UpdateDisplayedEntries()
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

static void HandeOutput(const char *output)
{
    if (output[0] == '\0')
        return;

    // TODO: Moar modez!
    if (dmenu_mode)
    {
        printf("%s\n", output);
    }
    else
    {
        // Iterate over desktop entries and find the matching one.
        for (int i = 0; i < desktop_entries_size; ++i)
            if (strcmp(output, StringGetText(&desktop_entries[i].name)) == 0)
            {
                printf(StringGetText(&desktop_entries[i].exec));
                // TODO: Calculate the length properly.
                char buffer[strlen(StringGetText(&desktop_entries[i].exec)) + strlen(" i3-msg exec ''")];
                buffer[0] = '\0';

                // TODO: Add ability to specify command by the user (use % to
                // replace a command to call).
                strcat(strcat(strcat(buffer, "i3-msg exec '"),
                              StringGetText(&desktop_entries[i].exec)), "'");
                system(buffer);
                break;
            }
    }
}

static void LoadEntriesFromStdin()
{
    size_t len = 256;
    int nread;

    // We allocate it once, so that we can reuse it. It must be allocated with
    // malloc because getline will try to reallocate it when gets more that out
    // character limit.
    char *line = malloc(sizeof(char) * 256);

    while ((nread = getline(&line, &len, stdin)) != -1)
    {
        fprintf(stderr, "Retrieved line of length %du:\n", nread);

        // Remove the newline, and calculate the size
        int text_len = 0;
        for (int i = 0; line[i] != '\0'; ++i)
            if (line[i] == '\n' && line[i+1] == '\0')
            {
                text_len = i;
                line[i] = ('\0');
                break;
            }

        AddEntry(line, text_len);
    }

    free(line);
}

// TODO: Investigate how does X want us to redraw the window, because redrawing
// it manually makes it weird and sometimes ignores what we just copied into GC.

// TODO: Refactor stderr printfs
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

// I get an implicite declaration here, so i have to define it manually.
int getline(char **lineptr, size_t *n, FILE *stream);

// ^Same here.
int nanosleep(const struct timespec *req, struct timespec *rem);

// TODO: deal with this!
static void AddEntry(char *value, int length);

// TODO: Menu settings.
static int case_sensitive = 0;
static int dmenu_mode = 0;

#include "x11draw.c"
#include "desktop_entry_parser.c"
#include "string.c"
#include "util.c"

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
            if (strcmp(output, desktop_entries[i].name) == 0)
            {
                printf(desktop_entries[i].exec);
                char buffer[strlen(desktop_entries[i].exec) + strlen(" &")];
                buffer[0] = '\0';
                // TODO: Add ability to specify command by the user (use % to
                // replace a command to call).
                strcat(strcat(buffer, "i3-msg exec "), desktop_entries[i].exec);
                system(buffer);
                break;
            }
    }
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

static void RedrawWindow()
{
    DrawBackground();
    int entry_idx = 0;
    Entry *current_entry = first_displayed_entry;

    // Skip first [current_select_offset] entries.
    for (int i = 0; i < current_select_offset; ++i)
        current_entry = current_entry->next_displayed;

    DrawInertedText(inserted_text);

    while (current_entry)
    {
        char *entry_value = StringGetText(&current_entry->text);

        int finished =
#if 1
        DrawEntry(entry_value, entry_idx,
                  current_entry == current_select);
#else
        DrawEntry("Here is some text", entry_idx,
                  current_entry == current_select);
#endif

        current_entry = current_entry->next_displayed;
        entry_idx++;

        if (finished)
            break;
    }

    DrawMarginLines();

#if 1
    if (current_select)
        fprintf(stderr,
                "CURRENT SELECTED OPTION TO COMPLETE: %s\n",
                StringGetText(&current_select->text));
    else
        fprintf(stderr, "NO COMPLETION AVAILABLE!\n");
#endif

    XFlush(dpy);
}

static void EventLoop()
{
    while (1)
    {
        int key_press = 0;

        XEvent e;
        XNextEvent(dpy, &e);
        if (e.type == KeyPress)
        {
            key_press++;
            char string[25];
            // int len;
            KeySym keysym;
            int redraw = 0;
            int reset_select = 0;

            XLookupString(&e.xkey, string, 25, &keysym, NULL);
            switch (keysym)
            {
                case XK_Escape:
                {
                    exit(12);
                }
                break;

                case XK_Tab:
                {
                    CompleteAtCurrent();
                    reset_select = 1;
                    redraw = 1;
                } break;

                case XK_BackSpace:
                {
                    if (inserted_text_idx)
                    {
                        inserted_text_idx--;
                        inserted_text[inserted_text_idx] = '\0';

                        reset_select = 1;
                        redraw = 1;
                    }
                }
                break;

                case XK_Down:
                {
                    if (current_select->next_displayed)
                    {
                        current_select = current_select->next_displayed;
                        current_select_idx++;

                        // First -1 because one field holds inserted
                        // text. Second -1 is because we want some space, and
                        // the last field might be cuted.
                        while (current_select_idx >= fields_drawned -1 -1)
                        {
                            current_select_offset++;
                            current_select_idx--;
                        }
                        redraw = 1;
                    }
                } break;

                case XK_Up:
                {
                    if (current_select->prev_displayed)
                    {
                        // If there is previous entry either current_select_idx
                        // or current_select_offset is > 0.
                        assert(current_select_offset > 0 || current_select_idx > 0);

                        current_select = current_select->prev_displayed;
                        current_select_idx > 0
                            ? current_select_idx--
                            : current_select_offset--;

                        redraw = 1;
                    }

                } break;

                case XK_Return:
                {
                    if (current_select)
                        CompleteAtCurrent();

                    return;
                }
                break;

                default:
                    break;
            }

            // TODO: Drawable characters!!!!
            if (32 <= string[0] && string[0] <= 126)
            {
                inserted_text[inserted_text_idx++] = string[0];
                inserted_text[inserted_text_idx] = '\0';
                redraw = 1;
                reset_select = 1;
            }

            if (reset_select)
            {
                UpdateDisplayedEntries();
            }

            if (redraw)
            {
                RedrawWindow();
                fprintf(stderr,
                        "CURRENT_SELECT_OFFSET: %d\nCURRENT_SELECT_IDX: %d\n",
                        current_select_offset,
                        current_select_idx);
            }
        }

        else if (e.type == ButtonPress)
        {
            XCloseDisplay(dpy);
            exit(2);
        }
        else if (e.type == Expose)
        {
            fprintf(stderr, "Exposed?\n");
        }
    }
}

// TODO: Add ability to specify 'dmenu-mode' and then read from stdin!
int main()
{
    // Inicialize the first block.
    first_entries_block.number_of_entries = 0;
    first_entries_block.next = NULL;

    if (dmenu_mode)
        LoadEntriesFromStdin();
    else
    {
        LoadEntriesFromDotDesktop("/usr/share/applications/",
                                  &desktop_entries,
                                  &desktop_entries_size);
    }

    inserted_text[0] = '\0';

    // TODO: Add option to sort entrues Lexicographically? Move it to dmenu mode
    // start if any.

    // qsort(entries, number_of_entries, sizeof(char *), LexicographicalCompare);

    DrawAndKeyboardInit();

    UpdateDisplayedEntries();
    RedrawWindow();

    EventLoop();

    XUngrabKeyboard(dpy, CurrentTime);

    HandeOutput(inserted_text);
    return 0;
}

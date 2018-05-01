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

// [text] must be \0 terminated.
int GetLettersCount(const char *text)
{
    for (int i = 0;; ++i)
        if (text[i] == '\0')
            return i;
}

#include "x11draw.c"

static int dmenu_mode = 0;

// Command used to run a program in a terminal. Not used in dmenu mode.
char *terminal_command = "gnome-terminal --";

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

static inline char *GetText(Entry *entry)
{
    return entry->large_text
        ? entry->large_text
        : &(entry->text[0]);
}

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

// I get an implicite declaration here, so i have to define it manually.
int getline(char **lineptr, size_t *n, FILE *stream);

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

static int SuffixMatch(const char *text, const char *pattern)
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

inline static int EntryMatch(const Entry *entry, const char *text)
{
    if (entry->large_text)
        return PrefixMatch(entry->large_text, text);
    else
        return PrefixMatch(entry->text, text);
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

typedef struct
{
    char *name;
    char *exec;
} DesktopEntry;

#define DESKTOP_NAME_FIELD "Name="
#define DESKTOP_NAME_FIELD_SIZE (5)
#define DESKTOP_TYPE_FIELD "Type="
#define DESKTOP_TYPE_FIELD_SIZE (5)
#define DESKTOP_EXEC_FIELD "Exec="
#define DESKTOP_EXEC_FIELD_SIZE (5)
#define DESKTOP_TERM_FIELD "Terminal="
#define DESKTOP_TERM_FIELD_SIZE (9)
#define DESKTOP_NO_DISPLAY_FIELD "NoDisplay="
#define DESKTOP_NO_DISPLAY_FIELD_SIZE (10)

char *DuplicateString(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    if(p) { strcpy(p, s); }
    return p;
}

DesktopEntry *desktop_entries;
int desktop_entries_size = 0;

void LoadEntriesFromDotDesktop(const char *path)
{
    DIR *dir;
    struct dirent *ent;
    char buffer[256];

    int result_idx = 0,
        result_max_len = 64;
    DesktopEntry *result = malloc(sizeof(DesktopEntry) * result_max_len);

    if ((dir = opendir(path)) != NULL)
    {
        char *line = malloc(sizeof(char) * 256);
        size_t len = 0;
        int nread;

        // print all the files and directories within directory
        while ((ent = readdir (dir)) != NULL)
            if (SuffixMatch(ent->d_name, ".desktop"))
            {
                buffer[0] = '\0';
                const char *file_path = strcat(strcat(buffer, path), ent->d_name);

                FILE *file = fopen(file_path, "r");
                assert(file);

                DesktopEntry info = { NULL, NULL };
                int terminal = 0; // Is app using terminal or not (TODO: tricky)
                int header_checked = 0; // There is [DesktopEntry] header.
                int ignore_this_entry = 0; // e.g. entry is not an application.

                while ((nread = getline(&line, &len, file)) != -1)
                {
                    if (strncmp(line, "[", 1) == 0)
                    {
                        if (strcmp(line, "[Desktop Entry]\n") == 0)
                        {
                            header_checked = 1;
                            continue;
                        }
                        else
                        {
                            // Skip other entires that [Desktop Entry]...
                            do
                            {
                                nread = getline(&line, &len, file);
                            } while (nread != -1 && strcmp(line, "[Desktop Entry]\n") != 0);

                            if (nread == -1)
                                break;
                        }
                    }

                    if (!header_checked)
                    {
                        // TODO: Handle this case better!
                        assert(strcmp(line, "[Desktop Entry]\n") == 0);
                        header_checked = 1;
                        continue;
                    }

                    if (strlen(line) > DESKTOP_TYPE_FIELD_SIZE
                        && strncmp(line, DESKTOP_TYPE_FIELD, DESKTOP_TYPE_FIELD_SIZE) == 0
                        && strcmp(line + DESKTOP_TYPE_FIELD_SIZE, "Application\n") != 0)
                    {
                        ignore_this_entry = 1;
                        break;
                    }
                    else if (strlen(line) > DESKTOP_NAME_FIELD_SIZE
                             && strncmp(line, DESKTOP_NAME_FIELD, DESKTOP_NAME_FIELD_SIZE) == 0)
                    {
                        char *name = line + DESKTOP_NAME_FIELD_SIZE;
                        if (name)
                            info.name = DuplicateString(name);
                    }
                    else if (strlen(line) > DESKTOP_EXEC_FIELD_SIZE
                             && strncmp(line, DESKTOP_EXEC_FIELD, DESKTOP_EXEC_FIELD_SIZE) == 0)
                    {
                        char *exec = line + DESKTOP_EXEC_FIELD_SIZE;
                        if (exec)
                            info.exec = DuplicateString(exec);
                    }
                    else if (strlen(line) > DESKTOP_TERM_FIELD_SIZE
                             && strncmp(line, DESKTOP_TERM_FIELD, DESKTOP_TERM_FIELD_SIZE) == 0)
                    {
                        char *term = line + DESKTOP_TERM_FIELD_SIZE;
                        if (strcmp(term, "true\n") == 0 || strcmp(term, "true") == 0)
                            terminal = 1;
                    }
                    else if (strlen(line) > DESKTOP_NO_DISPLAY_FIELD_SIZE
                             && strncmp(line, DESKTOP_NO_DISPLAY_FIELD, DESKTOP_NO_DISPLAY_FIELD_SIZE) == 0)
                    {
                        char *display_info = line + DESKTOP_NO_DISPLAY_FIELD_SIZE;
                        if (display_info
                            && (strcmp(display_info, "true\n") == 0
                                || strcmp(display_info, "true") == 0))
                            ignore_this_entry = 1;
                    }
                }

                if (header_checked && !ignore_this_entry && info.name && info.exec)
                {
                    if (terminal)
                    {
                        int termnial_command_size = strlen(terminal_command),
                            exec_original_size = strlen(info.exec);

                        info.exec = realloc(info.exec, sizeof(char) *
                                            (exec_original_size + termnial_command_size + 1));

                        memcpy(info.exec + termnial_command_size + 1,
                               info.exec,
                               sizeof(char) * exec_original_size);

                        for (int i = 0; i < termnial_command_size; ++i)
                            info.exec[i] = terminal_command[i];

                        info.exec[termnial_command_size] = ' ';

                    }

                    // Remove %'s:
                    int fixed_idx = 0;
                    for (int i = 0;
                         info.exec[i] != '\0' && info.exec[i] != '\n';
                         ++i)
                    {
                        if (info.exec[i] == '%'
                            && info.exec[i+1] != '\0'
                            && info.exec[i+1] != '\n')
                        {
                            i += 2;
                        }

                        info.exec[fixed_idx++] = info.exec[i];
                    }
                    info.exec[fixed_idx] = '\0';

                    // Remove newline from the name:
                    for (int i = 0; info.name[i] != '\0'; ++i)
                        if (info.name[i] == '\n')
                        {
                            info.name[i] = '\0';
                            break;
                        }

                    // Remove newline from the name:
                    for (int i = 0; info.exec[i] != '\0'; ++i)
                        if (info.exec[i] == '\n')
                        {
                            info.exec[i] = '\0';
                            break;
                        }

                    if (result_idx >= result_max_len)
                    {
                        result_max_len *= 2;
                        result = realloc(result, sizeof(DesktopEntry) * result_max_len);
                    }

                    result[result_idx++] = (DesktopEntry){ info.name, info.exec };
                }

                fclose(file);
            }

        closedir(dir);
    }


    int CompareNameLex (const void * a, const void * b)
    {
        return strcmp(((DesktopEntry *)a)->name, ((DesktopEntry *)b)->name);
    }

    // TODO: Do we need to sort?
    qsort(result, result_idx, sizeof(DesktopEntry), CompareNameLex);

    for (int i = 0; i < result_idx; ++i)
        AddEntry(result[i].name, strlen(result[i].name));

    desktop_entries = result;
    desktop_entries_size = result_idx;
}

void LoadEntriesFromStdin()
{
    size_t len = 256;
    int nread;

    // We allocate it once, so that we can reuse it.
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

    // TODO: line leaks, but nobody cares. It must be allocated with malloc
    // because getline will try to reallocate it when gets more that out
    // character limit.
}

void HandeOutput(const char *output)
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
        for (int i = 0; i < desktop_entries_size; ++i)
        {
            if (strcmp(output, desktop_entries[i].name) == 0)
                printf(desktop_entries[i].exec);
        }
    }
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

void RedrawWindow()
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
        char *entry_value = GetText(current_entry);

        int finished = DrawEntry(entry_value, entry_idx, current_entry == current_select);

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
                GetText(current_select));
    else
        fprintf(stderr, "NO COMPLETION AVAILABLE!\n");
#endif

    XFlush(dpy);
}

void EventLoop()
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
        LoadEntriesFromDotDesktop("/usr/share/applications/");
    }

    inserted_text[0] = '\0';

    // TODO: Add option to sort entrues Lexicographically? Move it to dmenu mode start if any.
    // qsort(entries, number_of_entries, sizeof(char *), LexicographicalCompare);

    DrawAndKeyboardInit();

    UpdateDisplayedEntries();
    RedrawWindow();

    EventLoop();

    XUngrabKeyboard(dpy, CurrentTime);

    HandeOutput(inserted_text);
    return 0;
}

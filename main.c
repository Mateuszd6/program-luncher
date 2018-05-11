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

// TODO: Deal with this! (The one and only call left is in the
// desktop entry parser).
static void AddEntry(char *value, int length);

#include "util.h"
#include "string.h"
#include "desktop_entry_parser.h"

// TODO: Menu settings.
static int case_sensitive = 0;
static int dmenu_mode = 0;

#include "menu.c"
#include "x11draw.c"
#include "desktop_entry_parser.c"
#include "string.c"
#include "util.c"

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

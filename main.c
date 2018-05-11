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
            UpdateMenuFeedback update_feedback = UMF_NONE;

            XLookupString(&e.xkey, string, 25, &keysym, NULL);
            switch (keysym)
            {
                case XK_Escape:
                    exit(4);
                    break;

                case XK_Tab:
                    update_feedback = MenuCompleteAtCurrent();
                    break;

                case XK_BackSpace:
                    update_feedback = MenuRemoveCharacter();
                    break;

                case XK_Down:
                    update_feedback = MenuMoveDown(fields_drawned);
                    break;

                case XK_Up:
                    update_feedback = MenuMoveUp();
                    break;

                // TODO: Move to menu.c
                case XK_Return:
                {
                    if (current_select)
                        MenuCompleteAtCurrent();

                    // This breaks the event loop by exitting this function.
                    return;
                }
                break;

                default:
                {
                    // TODO: Drawable characters!!!!
                    if (32 <= string[0] && string[0] <= 126)
                    {
                        update_feedback = MenuAddCharacter(string[0]);
                    }
                } break;
            }

            redraw = update_feedback & UMF_REDRAW;
            reset_select = update_feedback & UMF_RESET_SELECT;

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

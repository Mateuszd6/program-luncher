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

// TODO: I get an implicite declaration here, so i have to define it manually.
int getline(char **lineptr, size_t *n, FILE *stream);

// TODO: ^Same here.
int nanosleep(const struct timespec *req, struct timespec *rem);

#include "util.h"
#include "string.h"
#include "desktop_entry_parser.h"
#include "x11draw.h"

// TODO: Menu settings.
static int case_sensitive = 0;

// TODO: move to menu:
typedef enum
{
    MM_DMENU = 0,
    MM_APP_LUNCHER = 1
} MenuMode;

// This is the default mode.
static MenuMode menu_mode = MM_APP_LUNCHER;

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
            int reset_append = 0;

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
            reset_append = update_feedback & UMF_RESET_AFTER_APPEND;

            if (reset_select)
            {
                UpdateDisplayedEntries();
            }

            if (reset_append)
            {
                UpdateDisplayedEntriesAfterAppendingCharacter();
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

static int ParseColor(const char *color, ColorData *res_data)
{
    if (color[0] != '#' || strlen(color) != 7)
    {
        return 0;
    }

    unsigned char result[3];
    for (int i = 0; i < 3; ++i)
    {
        int value[2];
        for (int j = 0; j < 2; ++j)
        {
            char character = color[1 + 2*i + j];

            // If not in rage 0-9 and (after ToLower) not in rage a-f.
            if (character < '0' || character > '9')
            {
                if (ToLower(character) < 'a' || ToLower(character) > 'f')
                    return 0;
                else
                    value[j] = character - 'a' + 10;
            }
            else
                value[j] = character - '0';
        }

        result[i] = value[0] * 16 + value[1];
    }

    (* res_data) = (ColorData) { result[0], result[1], result[2] };
    return 1;
}

// TODO: Add ability to specify 'dmenu-mode' and then read from stdin!
int main(int argc, char **argv)
{
    void GetCurrentArgAsColorAndAssingIt(const int arg_idx, const int arg_count,
                                         ColorData *color_to_assing_ptr)
    {
        if (arg_idx >= arg_count)
        {
            // TODO: print usage...
            fprintf(stderr, "Bad arguments!\n");
            exit(1);
        }

        const char *color = argv[arg_idx];

        if (!ParseColor(color, color_to_assing_ptr))
        {
            fprintf(stderr, "Not supported color format!\n");
            exit(1);
        }
    }

    // TODO: This is a placeholder for the real argument parser.
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--dmenu") == 0)
            menu_mode = MM_DMENU;
        else if (strcmp(argv[i], "--app-luncher") == 0)
            menu_mode = MM_APP_LUNCHER;
        else if (strcmp(argv[i], "-nb") == 0 || strcmp(argv[i], "--nb") == 0)
        {
            ++i;
            GetCurrentArgAsColorAndAssingIt(i, argc, &bg_color_data);
        }
        else if (strcmp(argv[i], "-nf") == 0 || strcmp(argv[i], "--nf") == 0)
        {
            ++i;
            GetCurrentArgAsColorAndAssingIt(i, argc, &font_color_data);
        }
        else if (strcmp(argv[i], "-sb") == 0 || strcmp(argv[i], "--sb") == 0)
        {
            ++i;
            GetCurrentArgAsColorAndAssingIt(i, argc, &selected_field_color_data);
        }
        else if (strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--sf") == 0)
        {
            ++i;
            GetCurrentArgAsColorAndAssingIt(i, argc, &selected_font_color_data);
        }
        else
        {
            fprintf(stderr, "No idea what does arg %s mean!\n", argv[i]);
            exit(1);
        }
    }

    // Inicialize the first block.
    first_entries_block.number_of_entries = 0;
    first_entries_block.next = NULL;

    MenuInit();

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

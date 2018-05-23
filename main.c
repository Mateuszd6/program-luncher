// TODO: Refactor stderr printfs. TO WHAT?
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

// TODO: If c11 is used instead of gnu11 nanosleep and getline with some random
// reason are not defined. So implicite declaration warrnings are thrown!

#include "desktop_entry_parser.h"
#include "string.h"
#include "util.h"
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

#include "desktop_entry_parser.c"
#include "menu.c"
#include "string.c"
#include "util.c"
#include "x11draw.c"

// TODO: Move it to draw file...!
static void RedrawWindow()
{
    DrawBackground();
    int entry_idx = 0;
    Entry *current_entry = first_displayed_entry;

    // Skip first [current_select_offset] entries.
    for (int i = 0; i < current_select_offset; ++i)
        current_entry = current_entry->next_displayed;

    DrawInertedTextAndPrompt(inserted_text);

    while (current_entry)
    {
        char *entry_value = StringGetText(&current_entry->text);
        int finished =
            DrawEntry(entry_value, entry_idx, current_entry == current_select);

        current_entry = current_entry->next_displayed;
        entry_idx++;

        if (finished)
            break;
    }

    DrawMarginLines();

#if 0
    if (current_select)
        fprintf(stderr, "CURRENT SELECTED OPTION TO COMPLETE: %s\n",
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

        int redraw = 0;
        int reset_select = 0;
        int reset_append = 0;

        XEvent e;
        XNextEvent(dpy, &e);
        if (e.type == KeyPress)
        {
            key_press++;
            char string[25];
            // int len;
            KeySym keysym;

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
                    // TODO: Another characters support?
                    if (32 <= string[0] && string[0] <= 126)
                    {
                        update_feedback = MenuAddCharacter(string[0]);
                    }
                }
                break;
            }

            redraw = update_feedback & UMF_REDRAW;
            reset_select = update_feedback & UMF_RESET_SELECT;
            reset_append = update_feedback & UMF_RESET_AFTER_APPEND;
        }

        else if (e.type == Expose)
        {
            fprintf(stderr, "Exposed?\n");
        }


        // TODO: move redraw and others here and do it here!

        // TODO: After each frame check if this is  a case.
        if (menu_mode == MM_APP_LUNCHER)
        {
            // TODO: this is probobly horriebly wrong:
            if (desktop_state.dirty)
            {
                // TODO: At least here we must lock the desktop_state!
                MakeEntryArrayFromDesktopStateData();

                desktop_state.dirty = 0;

                pthread_create(&save_desktop_entries_thread, NULL,
                               SaveDesktopEntriesThreadMain, NULL);

                redraw = 1;
                reset_select = 1;
            }
        }


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
#if 0
            fprintf(stderr,
                    "CURRENT_SELECT_OFFSET: %d\nCURRENT_SELECT_IDX: %d\n",
                    current_select_offset, current_select_idx);
#endif
        }
    }
}

static int ParseColor(const char *color, ColorData *res_data)
{
    if (color[0] != '#' || strlen(color) != 7)
        return 0;

    unsigned char result[3];
    for (int i = 0; i < 3; ++i)
    {
        int value[2];
        for (int j = 0; j < 2; ++j)
        {
            unsigned char character = color[1 + 2 * i + j];

            // If not in rage 0-9 and (after ToLower) not in rage a-f.
            if (character < '0' || character > '9')
            {
                if (ToLower(character) < 'a' || ToLower(character) > 'f')
                    return 0;
                else
                    value[j] = ToLower(character) - 'a' + 10;
            }
            else
                value[j] = character - '0';
        }

        result[i] = value[0] * 16 + value[1];
    }

    // --sb \#47198D <- With this arg.
    // #47198D       <- This should come up.
    (*res_data) = (ColorData){result[0], result[1], result[2]};
    return 1;
}

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

    for (int i = 1; i < argc; ++i)
    {
        // Select the mode for the menu:
        if (strcmp(argv[i], "--dmenu") == 0)
            menu_mode = MM_DMENU;
        else if (strcmp(argv[i], "--app-luncher") == 0)
            menu_mode = MM_APP_LUNCHER;

        // Some graphical options:
        // TODO: Assert in these there IS a next arg!
        else if (strcmp(argv[i], "--no-lines") == 0)
            draw_lines = 0;
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
            GetCurrentArgAsColorAndAssingIt(i, argc,
                                            &selected_field_color_data);
        }
        else if (strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--sf") == 0)
        {
            ++i;
            GetCurrentArgAsColorAndAssingIt(i, argc, &selected_font_color_data);
        }

        // Option to add aditional entires or blacklist some of the existing ones.
        // TODO: Cannot blacklist manually added entries. Is it ok?
        // TODO: Make sure in these there IS a next arg!
        else if (strcmp(argv[i], "--appluncher-blacklist") == 0)
        {
            ++i;
            if (blacklisted_entries_capacity == blacklisted_entries_number)
            {
                blacklisted_entries_capacity =
                    MAX(blacklisted_entries_capacity, 4) * 2;
                blacklisted_entries = realloc(blacklisted_entries,
                                              blacklisted_entries_capacity);
            }

            assert(blacklisted_entries_capacity > blacklisted_entries_number);
            // TODO: Duplicate string or not?
            blacklisted_entries[blacklisted_entries_number++] = argv[i];
        }
        else if (strcmp(argv[i], "--appluncher-add") == 0)
        {
            ++i;
            if (extra_entries_capacity == extra_entries_number)
            {
                extra_entries_capacity = MAX(extra_entries_capacity, 4) * 2;

                extra_entries_names = realloc(extra_entries_names, extra_entries_capacity);
                extra_entries_execs = realloc(extra_entries_execs, extra_entries_capacity);
            }

            assert(extra_entries_capacity > extra_entries_number);
            // TODO: Decide if Duplicatestring is needed or we can just app a
            // poitner to an arg.
            extra_entries_names[extra_entries_number] = argv[i];
            ++i;
            // TODO: ^Same
            extra_entries_execs[extra_entries_number] = argv[i];
            extra_entries_number++;
        }

        // Set up prompt:
        else if (strcmp(argv[i], "--prompt") == 0 || strcmp(argv[i], "-p") == 0)
        {
            // TODO: Assert in these there IS a next arg!
            ++i;

            int length = strlen(argv[i]);
            assert(length < 255);
            for (int j = 0; j < length; ++j)
                prompt[j] = argv[i][j];
            prompt[length] = '\0';
            prompt_length = length;
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

    DrawAndKeyboardInit();
    UpdateDisplayedEntries();
    RedrawWindow();

    EventLoop();

    XUngrabKeyboard(dpy, CurrentTime);
    HandeOutput(inserted_text);


    return 0;
}

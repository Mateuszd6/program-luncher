// TODO: Investigate how does X want us to redraw the window, because
// redrawing it manually makes it weird and sometimes ignores what we just
// copied into GC.

// TODO: Refactor stderr printfs

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "menu.h"
#include "x11draw.h"

ssize_t getline(char **lineptr, size_t *n, FILE *stream);

static int dmenu_mode = 1;

static void LoadEntriesFromStdin()
{
    size_t len = 256;
    ssize_t nread;

    // We allocate it once, so that we can reuse it.
    char *line = malloc(sizeof(char) * 256);

    // TODO: Getline is not a standard, I get implicte declaration here,
    // that forces me tu define GNU_SOURCE.

    // NOTE: passing NULL and 0 t getline will make function allocate for us a
    // string.
    while ((nread = getline(&line, &len, stdin)) != -1)
    {
        fprintf(stderr, "Retrieved line of length %zu:\n", nread);
        // fwrite(line, nread, 1, stdout);

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

// TODO: How will the app check if stdin is redirected?
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

    }

    inserted_text[0] = '\0';

    // qsort(entries, number_of_entries, sizeof(char *), LexicographicalCompare);
    DrawAndKeyboardInit();

    UpdateDisplayedEntries();
    RedrawWindow();

    EventLoop();

    XUngrabKeyboard(dpy, CurrentTime);
    return 0;
}

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

// I get an implicite declaration here, so i have to define it manually.
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

static int dmenu_mode = 1;

static void LoadEntriesFromStdin()
{
    size_t len = 256;
    ssize_t nread;

    // We allocate it once, so that we can reuse it.
    char *line = malloc(sizeof(char) * 256);

    while ((nread = getline(&line, &len, stdin)) != -1)
    {
        fprintf(stderr, "Retrieved line of length %zu:\n", nread);

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

    // TODO: Add option to sort entrues Lexicographically?
    // qsort(entries, number_of_entries, sizeof(char *), LexicographicalCompare);
    DrawAndKeyboardInit();

    UpdateDisplayedEntries();
    RedrawWindow();

    EventLoop();

    XUngrabKeyboard(dpy, CurrentTime);
    return 0;
}

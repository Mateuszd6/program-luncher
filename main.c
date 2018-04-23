// TODO: Investigate how does X want us to redraw the window, because
// redrawing it manually makes it weird and sometimes ignores what we just
// copied into GC.

// TODO: Refactor stderr printfs

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h> // Every Xlib program must include this
#include <X11/Xutil.h>

#define WINDOW_W (640)
#define WINDOW_H (256)

int nanosleep(const struct timespec *req, struct timespec *rem);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

static Display *dpy;
static Window w;
static Screen *s;
static XFontStruct *font;
static GC gc;
static int whiteColor, blackColor;
#if 0
static Pixmap pixmap;
#endif
static Colormap color_map;

static XColor bg_color[2];
static XColor font_color;
static XColor lines_color;
static XColor selected_field_color;
static XColor selected_font_color;

// NOTE: PROGRAM OPTIONS:
static int dmenu_mode = 1; // Read from stdin, write to stdout.
static int case_sensitive = 0;

typedef struct
{
    int red;
    int green;
    int blue;
} ColorData;

// TODO: Add ability to specify colors by the user.
static const ColorData bg_color_data = {0x1e, 0x1f, 0x1c};
static const ColorData lines_color_data = {0x02, 0x02, 0x02};
static const ColorData font_color_data = {0xCF, 0xCF, 0xCF};
static const ColorData selected_field_color_data = {0xFF, 0x00, 0x00};
static const ColorData selected_font_color_data = {0xFF, 0xFF, 0xFF};

static int draw_lines = 1;

static char inserted_text[256];
static int inserted_text_idx = 0;

// TODO: SUMMART!
#define CHARACTERS_IN_SHORT_TEXT (48)

struct _Entry
{
    char text[CHARACTERS_IN_SHORT_TEXT];
    char *large_text;
    struct _Entry *next_displayed;
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

#if 0
static int number_of_entries = 0;
static char *entries[NUMBER_OF_ENTRIES_IN_BLOCK];
#endif

// The first block which will be usually enought for all entries.
// If there are dozens of them, next blocks will be allocated.
static EntriesBlock first_entries_block = {};
static EntriesBlock *last_block = &first_entries_block;

static Entry *first_displayed_entry = NULL;

// A pointer to the selected entry, must be in a printed list!
static Entry *current_select = NULL;

#if 0
// TODO: refactor this!
static char *displayed_entries[NUMBER_OF_ENTRIES_IN_BLOCK];
static int displayed_entries_size = 0;
#endif

static struct
{
    int direction;
    int ascent;
    int descent;
    XCharStruct overall;
} font_info;

static void SetUpFont()
{
    // "-*-helvetica-*-r-*-*-18-*-*-*-*-*-*-*";

    // NOTE: With fixed it appears there is no size > 20 (at least at my PC).
    const char *fontname = "-*-fixed-*-*-*-*-20-*-*-*-*-*-*-*";
    font = XLoadQueryFont(dpy, fontname);

    // If the font could not be loaded, revert to the "fixed" font.
    if (!font)
    {
        fprintf(stderr, "unable to load font %s: using fixed\n", fontname);
        font = XLoadQueryFont(dpy, "fixed");
    }
    XSetFont(dpy, gc, font->fid);
}

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
static int GetLettersCount(const char *text)
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

static void UpdateDisplayedEntries()
{
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
                    prev_displayed_entry->next_displayed = curr_entry;
                else
                    first_displayed_entry = curr_entry;

                prev_displayed_entry = curr_entry;
            }
        }
        current_block = current_block->next;
    }
    while (current_block);
}

inline static char *GetText(Entry *entry)
{
    return entry->large_text ? entry->large_text : &(entry->text[0]);
}

static void RedrawWindow()
{
    // Draw the background.
    XSetForeground(dpy, gc, bg_color[0].pixel);
    XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, WINDOW_H);

    int x = 0;
    int y = 0;

    x = 5;  // (WINDOW_W - overall.width) / 2;
    y = 20; // WINDOW_H / 2 + (ascent - descent) / 2;

    XSetForeground(dpy, gc, font_color.pixel);
    XDrawString(dpy, w, gc, x, y, inserted_text, inserted_text_idx);

    // TODO: Calculate the number of rects to draw within a window!!
    for (int i = 0; i < 100; ++i)
    {
        XSetForeground(dpy, gc, (i & 1 ? bg_color[0].pixel : bg_color[1].pixel));
        XFillRectangle(dpy, w, gc, 0,
                       y + i * (font_info.ascent - font_info.descent + 10) + 5,
                       WINDOW_W, font_info.ascent - font_info.descent + 10);
    }

    UpdateDisplayedEntries();

#if 0
    displayed_entries_size = 0;
    for (int i = 0; i < number_of_entries; ++i)
        if (inserted_text[0] == '\0'
            || PrefixMatch(entries[i], inserted_text))
        {
            displayed_entries[displayed_entries_size++] = entries[i];
        }
#endif

    int entry_idx = 0;
    Entry *current_entry = first_displayed_entry;
    while (current_entry)
    {
        char *entry_value = GetText(current_entry);

        XSetForeground(dpy, gc, font_color.pixel);
        if (current_entry == current_select)
        {
            XSetForeground(dpy, gc, selected_field_color.pixel);
            XFillRectangle(dpy, w, gc, 0,
                           y + entry_idx * (font_info.ascent - font_info.descent + 10) + 5,
                           WINDOW_W, font_info.ascent - font_info.descent + 10);
        }

        XSetForeground(dpy, gc, selected_font_color.pixel);
        XDrawString(dpy, w, gc, x,
                    y + (entry_idx + 1) * (font_info.ascent - font_info.descent + 10),
                    entry_value, GetLettersCount(entry_value));

        current_entry = current_entry->next_displayed;
        entry_idx++;
    }

    // Draw the margins:
    if (draw_lines)
    {
        // TODO: Change the color to line color!
        XSetForeground(dpy, gc, lines_color.pixel);
        XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, 1);
        XFillRectangle(dpy, w, gc, 0, 0, 1, WINDOW_H);
        XFillRectangle(dpy, w, gc, WINDOW_W - 1, 0, WINDOW_W, WINDOW_H);
        XFillRectangle(dpy, w, gc, 0, WINDOW_H - 1, WINDOW_W, WINDOW_H);

        for (int i = 0; i < 100; ++i)
            XFillRectangle(dpy, w, gc, 0,
                           y + i * (font_info.ascent - font_info.descent + 10) + 5,
                           WINDOW_W, 1);
    }

    // TODO: Or <=?
    // TODO: If no entries this assertion is broken. Pattern when
    // [number_of_entries] == 0?
    // assert(current_select < number_of_entries); // TODO: BRING BACK THIS ASSERTION!
#if 0
    fprintf(stderr, "CURRENT SELECTED OPTION TO COMPLETE: %s\n",
            GetText(current_select));
#endif
    XFlush(dpy);

#if 0
    fprintf(stderr, inserted_text);
    fprintf(stderr, "\n");
#endif
}

static void CompleteAtCurrent()
{
    char *current_complete = GetText(current_select);
    int curr_complete_size = GetLettersCount(current_complete);
    for (int i = 0; i < curr_complete_size; ++i)
        inserted_text[i] = current_complete[i];
    inserted_text_idx = curr_complete_size;
}

static void AllocateXColorFromColorData(XColor *xcolor,
                                        const ColorData data)
{
    xcolor->red = data.red * 256;
    xcolor->green = data.green * 256;
    xcolor->blue = data.blue * 256;
    xcolor->flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, color_map, xcolor);
}

#if 0
static int LexicographicalCompare(const void *a, const void *b)
{
    // TODO: Support case (in)sensitivness!
    const char *pa = *(const char**)a;
    const char *pb = *(const char**)b;
    return strcmp(pa,pb);
}
#endif

// [value] must be copied!
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

        len = 0;
        line = NULL;
    }

    // TODO: line leaks, but nobody cares. It must be allocated with malloc
    // because getline will try to reallocate it when gets more that out
    // character limit.
}

// TODO: How will the app check if stdin is redirected?
// TODO: Add ability to specify 'dmenu-mode' and then read from stdin!
int main()
{
    // TODO: Not sure if this is a must-do.
    first_entries_block.number_of_entries = 0;
    first_entries_block.next = NULL;

    if (dmenu_mode)
    {
        // TODO: This hack tries to guess if input is redirected.
        // Possibly use as a cool feature:
#if 0
        if (isatty(STDIN_FILENO))
            number_of_entries = 0;
        else
            LoadEntriesFromStdin();
#else
        LoadEntriesFromStdin();
#endif
    }

#if 0
    inserted_text[0] = '\0';
#else
    inserted_text[0] = 'F';
    inserted_text[1] = '\0';
#endif

    dpy = XOpenDisplay(NULL);
    assert(dpy);

    // Initialize color map.
    color_map = XDefaultColormap(dpy, 0);
    blackColor = BlackPixel(dpy, DefaultScreen(dpy));
    whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

    ColorData second_bg_color_data = {
        bg_color_data.red-0x08,
        bg_color_data.green-0x08,
        bg_color_data.blue-0x08
    };

    AllocateXColorFromColorData(bg_color, bg_color_data);
    AllocateXColorFromColorData(bg_color + 1, second_bg_color_data);
    AllocateXColorFromColorData(&font_color, font_color_data);
    AllocateXColorFromColorData(&lines_color, lines_color_data);
    AllocateXColorFromColorData(&selected_field_color, selected_field_color_data);
    AllocateXColorFromColorData(&selected_font_color, selected_font_color_data);

    // MY:
    XSetWindowAttributes wa;
    wa.override_redirect = True;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

    // Create the window
    int scr = DefaultScreen(dpy);
    s = ScreenOfDisplay(dpy, scr);

    w = XCreateWindow(
        dpy, DefaultRootWindow(dpy), (WidthOfScreen(s) - WINDOW_W) / 2,
        (HeightOfScreen(s) - WINDOW_H) / 2, WINDOW_W, WINDOW_H, 0,
        DefaultDepth(dpy, scr), InputOutput, DefaultVisual(dpy, scr),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XClearWindow(dpy, w);

    // TODO: Copied from dmenu.
    struct timespec tim;
    tim.tv_sec = 0;
    tim.tv_nsec = 1000 * 1000 * 10;

    int grab_succeded = 0;
    for (int i = 0; i < 1000; ++i)
    {
        if (!XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync,
                           GrabModeAsync, CurrentTime))
        {
            grab_succeded = 1;
            break;
        }
        nanosleep(&tim, NULL);
    }

    if (!grab_succeded)
    {
        // TODO: Remove this funny kind of obsolete stuff with logging into
        // weird files.
        fprintf(stderr, "Shit!\n");
        system("echo bad > /home/mateusz/log.ALuncher");
        exit(3);
    }

    // For testing stuff that does not work with keyboard nor needs it.
#if 0
    XUngrabKeyboard(dpy, CurrentTime);
#endif

    XChangeWindowAttributes(dpy, w, 0, &wa);

    // We want to get MapNotify events
    XSelectInput(dpy, w, StructureNotifyMask);

    // "Map" the window (that is, make it appear on the screen)
    XMapWindow(dpy, w);

    // Create a "Graphics Context"
    gc = XCreateGC(dpy, w, 0, NULL);
#if XSetForeground
    // Tell the GC we draw using the white color
    0emacs(dpy, gc, blackColor);

    // TODO: Why is is not working?
    XSetBackground(dpy, gc, whiteColor);
#endif

    // TODO: DOnt need them here:
    XSetForeground(dpy, gc, blackColor);

#if 1
    // I guess XParseColor will work here
    XSetForeground(dpy, gc, bg_color[0].pixel);
#endif
    XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, WINDOW_H);

    SetUpFont();
    // Calculate font-related stuff.
    XTextExtents(font, "Hello World!", GetLettersCount("Hello World!"),
                 &font_info.direction, &font_info.ascent,
                 &font_info.descent, &font_info.overall);

    XFlush(dpy);

    // qsort(entries, number_of_entries, sizeof(char *), LexicographicalCompare);

#if 1
    // a = characters[0][1];

    // TODO: PRobobly not the best way, but who cares?
    // int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    // pixmap = XCreatePixmapFromBitmapData(dpy, w, (char *)a.pixels, a.w, a.h,
    // whiteColor, blackColor, depth);

    // XFlush(dpy);

    // XCreatePixmap(dpy, DefaultRootWindow(dpy), a.w, a.h, depth);
    // for (int i = 0; i < a.w; ++i)
    // for (int j = 0; j < a.h; ++j)
    // XDrawPoint(dpy, pixmap, gc, i, j);
#endif

    RedrawWindow();

    // Wait for the MapNotify event
    while (1)
    {
        int key_press = 0;

        XEvent e;
        XNextEvent(dpy, &e);
        // if (e.type == MapNotify)
        //    break;
        // else
        if (e.type == KeyPress)
        {
            key_press++;
            char string[25];
            // int len;
            KeySym keysym;
            int redraw = 0;

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
                    assert(0);
                    CompleteAtCurrent();
                    current_select = 0;
                    redraw = 1;
                } break;

                case XK_BackSpace:
                {
                    if (inserted_text_idx)
                    {
                        inserted_text_idx--;
                        inserted_text[inserted_text_idx] = '\0';
                        redraw = 1;
                    }
                }
                break;

                // TODO: Assert that it is smaller that the total number of
                // displayed entries!
                case XK_Down:
                {
                    assert(0);
                    //if (current_select < displayed_entries_size - 1)
                    {
                        current_select++;
                        redraw = 1;
                    }
                } break;

                // TODO: Assert that it is > 0!
                case XK_Up:
                {
                    assert(0);
                    // if (current_select > 0)
                    {
                        current_select--;
                        redraw = 1;
                    }
                } break;

                case XK_Return:
                {
                    // If there are entries to complete we complete the first one.
                    // if (displayed_entries_size > 0)
                    {
                        CompleteAtCurrent();
                        current_select = 0; // TODO: probobly not necesarry.
                    }

                    printf(inserted_text);
                    putchar('\n');
                    XUngrabKeyboard(dpy, CurrentTime);
                    exit(0);
                }
                break;

                default:
                    break;
            }
#if 0
            fprintf(stderr, "String: %s|\n", string);
#endif
            // TODO: Drawable characters!!!!
            if (32 <= string[0] && string[0] <= 126)
            {
                inserted_text[inserted_text_idx++] = string[0];
                inserted_text[inserted_text_idx] = '\0';
                redraw = 1;
                current_select = 0;
            }

            if (redraw)
            {
                // TODO: Update menu might have different conditions that
                // redraw!
#if 0
                UpdateMenu();
#endif
                RedrawWindow();
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

    XUngrabKeyboard(dpy, CurrentTime);
    return 0;
}

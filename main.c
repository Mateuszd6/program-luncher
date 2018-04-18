// TODO(Mateusz): Investigate how does X want us to redraw the window, because
// redrawing it manually makes it weird and sometimes ignores what we just
// copied into GC.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h> // Every Xlib program must include this
#include <X11/Xutil.h>

#include <assert.h> // I include this to test return values the lazy way
#include <unistd.h>

#define WINDOW_W (640)
#define WINDOW_H (256)

int nanosleep(const struct timespec *req, struct timespec *rem);

Display *dpy;
Window w;
Screen *s;
XFontStruct *font;
GC gc;
int whiteColor, blackColor;
Pixmap pixmap;
Colormap color_map;

XColor bg_color[2];
XColor font_color;
XColor lines_color;
XColor selected_field_color;
XColor selected_font_color;

static int current_select = 0;

typedef struct
{
    int red;
    int green;
    int blue;
} ColorData;

const ColorData bg_color_data = {0x1e, 0x1f, 0x1c};
const ColorData lines_color_data = {0x02, 0x02, 0x02};
const ColorData font_color_data = {0xCF, 0xCF, 0xCF};
const ColorData selected_field_color_data = {0xFF, 0x00, 0x00};
const ColorData selected_font_color_data = {0x00, 0xFF, 0xFF};

static int draw_lines = 1;

static void set_up_font()
{
    const char *fontname =
        "-*-fixed-*-*-*-*-20-*-*-*-*-*-*-*"; // "-*-helvetica-*-r-*-*-18-*-*-*-*-*-*-*";
    font = XLoadQueryFont(dpy, fontname);

    /* If the font could not be loaded, revert to the "fixed" font. */
    if (!font)
    {
        fprintf(stderr, "unable to load font %s: using fixed\n", fontname);
        font = XLoadQueryFont(dpy, "fixed");
    }
    XSetFont(dpy, gc, font->fid);
}

char inserted_text[256];
int inserted_text_idx = 0;

#define NUMBER_OF_ENTRIES (25)
const char *entries[NUMBER_OF_ENTRIES] = {
    "Foo",
    "Bar",
    "FooBar",
    "Scoo",
    "Test",

    "Another string",
    "More text",
    "Even more text",
    "Options options options",
    "More options",

    "Text, text, text",
    "Trololololo ",
    "Blah, Blah, Blah, Blah, Blah, Blah, ",
    "Teeeeeext",
    "This is even more stupid text",

    "Another string",
    "More text",
    "Even more text",
    "Options options options",
    "More options",

    "Foo again",
    "Bar again",
    "FooBar again",
    "Scoo again",
    "Test again"};

static int prefixMatch(const char *text, const char *pattern)
{
    for (int i = 0;; ++i)
    {
        if (pattern[i] == '\0')
            return 1;

        if (text[i] == '\0' || text[i] != pattern[i])
            return 0;
    }
}

// [text] must be \0 terminated.
static int getLettersCount(const char *text)
{
    for (int i = 0;; ++i)
        if (text[i] == '\0')
            return i;
}

// This sets up [curret_select] to the first match.
static void updateMenu()
{
    for (int i = 0; i < NUMBER_OF_ENTRIES; ++i)
        if (prefixMatch(entries[i], inserted_text))
        {
            current_select = i;
            return;
        }
}

static void redrawWindow()
{
    // Draw the background.
    XSetForeground(dpy, gc, bg_color[0].pixel);
    XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, WINDOW_H);

    int x = 0;
    int y = 0;
    int direction = 0;
    int ascent = 0;
    int descent = 0;
    XCharStruct overall;

    XSetForeground(dpy, gc, whiteColor);
    XTextExtents(font, "Hello World!", getLettersCount("Hello World!"),
                 &direction, &ascent, &descent, &overall);

    x = 5;  // (WINDOW_W - overall.width) / 2;
    y = 20; // WINDOW_H / 2 + (ascent - descent) / 2;

    XSetForeground(dpy, gc, font_color.pixel);
    XDrawString(dpy, w, gc, x, y, inserted_text, inserted_text_idx);

    for (int i = 0; i < 100; ++i)
    {
        XSetForeground(dpy, gc, (i & 1 ? bg_color[0].pixel : bg_color[1].pixel));
        XFillRectangle(dpy, w, gc, 0, y + i * (ascent - descent + 10) + 5,
                       WINDOW_W, ascent - descent + 10);
    }

    int displayed_entries = 1;
    for (int i = 0; i < NUMBER_OF_ENTRIES; ++i)
    {
        if (inserted_text[0] == '\0' || prefixMatch(entries[i], inserted_text))
        {
            XSetForeground(dpy, gc, font_color.pixel);
            if (displayed_entries == 1)
                XSetForeground(dpy, gc, selected_font_color.pixel);

            XDrawString(dpy, w, gc, x,
                        y + displayed_entries * (ascent - descent + 10),
                        entries[i], getLettersCount(entries[i]));

            displayed_entries++;
        }
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
            XFillRectangle(dpy, w, gc, 0, y + i * (ascent - descent + 10) + 5,
                           WINDOW_W, 1);
    }

    // TODO: Or <=?
    assert(current_select < NUMBER_OF_ENTRIES);
    printf("CURRENT SELECTED OPTION TO COMPLETE: %s\n",
        entries[current_select]);


    XFlush(dpy);

#if 0
    printf(inserted_text);
    printf("\n");
#endif
}

void allocateXColorFromColorData(XColor *xcolor, const ColorData data)
{
    xcolor->red = data.red * 256;
    xcolor->green = data.green * 256;
    xcolor->blue = data.blue * 256;
    xcolor->flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, color_map, xcolor);
}

int main()
{
    inserted_text[0] = '\0';

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

    allocateXColorFromColorData(bg_color, bg_color_data);
    allocateXColorFromColorData(bg_color + 1, second_bg_color_data);
    allocateXColorFromColorData(&font_color, font_color_data);
    allocateXColorFromColorData(&lines_color, lines_color_data);
    allocateXColorFromColorData(&selected_field_color, selected_field_color_data);
    allocateXColorFromColorData(&selected_font_color, selected_font_color_data);

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
        // FAILED!
        printf("Shit!\n");
        system("echo bad > /home/mateusz/log.log");
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
#if 0
    // Tell the GC we draw using the white color
    XSetForeground(dpy, gc, blackColor);

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

    set_up_font();

    XFlush(dpy);

#if 1
    // a = characters[0][1];

    // TODO: PRobobly not the best way, but who cares?
    // int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    /* pixmap = XCreatePixmapFromBitmapData(dpy, w, (char *)a.pixels, a.w, a.h,
     */
    /* whiteColor, blackColor, depth); */

    // XFlush(dpy);

    /* XCreatePixmap(dpy, DefaultRootWindow(dpy), a.w, a.h, depth); */
    /* for (int i = 0; i < a.w; ++i) */
    /* for (int j = 0; j < a.h; ++j) */
    /* XDrawPoint(dpy, pixmap, gc, i, j); */
#endif

    redrawWindow();

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

                case XK_Return:
                {
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
            printf("String: %s|\n", string);
#endif
            // TODO: Drawable characters!!!!
            if (32 <= string[0] && string[0] <= 126)
            {
                inserted_text[inserted_text_idx++] = string[0];
                inserted_text[inserted_text_idx] = '\0';
                redraw = 1;
            }

            if (redraw)
            {
                // TODO: Update menu might have different conditions that
                // redraw!
                updateMenu();
                redrawWindow();
            }
        }

        else if (e.type == ButtonPress)
        {
            XCloseDisplay(dpy);
            exit(2);
        }
        else if (e.type == Expose)
        {
            printf("Exposed?\n");
        }
    }

    XUngrabKeyboard(dpy, CurrentTime);
    return 0;
}

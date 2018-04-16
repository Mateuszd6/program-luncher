// TODO(Mateusz): Investigate how does X want us to redraw the window, because
// redrawing it manually makes it weird and sometimes ignores what we just
// copied into GC.

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <X11/Xlib.h> // Every Xlib program must include this
#include <X11/Xutil.h>

#include <assert.h> // I include this to test return values the lazy way
#include <unistd.h>

#define WINDOW_W (640)
#define WINDOW_H (125)

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

const int bg_red = 0x1e, bg_green = 0x1f, bg_blue = 0x1c;

static void
set_up_font ()
{
    const char * fontname = "-*-fixed-*-*-*-*-20-*-*-*-*-*-*-*"; // "-*-helvetica-*-r-*-*-18-*-*-*-*-*-*-*";
    font = XLoadQueryFont (dpy, fontname);

    /* If the font could not be loaded, revert to the "fixed" font. */
    if (! font) {
        fprintf (stderr, "unable to load font %s: using fixed\n", fontname);
        font = XLoadQueryFont (dpy, "fixed");
    }
    XSetFont (dpy, gc, font->fid);
}

char inserted_text[256];
int inserted_text_idx = 0;

#define NUMBER_OF_ENTRIES (25)
const char *entries[NUMBER_OF_ENTRIES] =
{
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
    "Test again"
};

static int
prefixMatch(const char *text, const char *pattern)
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
static int
getLettersCount(const char *text)
{
    for (int i = 0;; ++i)
        if (text[i] == '\0')
            return i;
}

static void
redrawWindow()
{
    // Draw the background.
    XSetForeground(dpy, gc, bg_color[0].pixel);
    XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, WINDOW_H);

    // Draw the margins:
    XSetForeground(dpy, gc, whiteColor);
    XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, 1);
    XFillRectangle(dpy, w, gc, 0, 0, 1, WINDOW_H);
    XFillRectangle(dpy, w, gc, WINDOW_W-1, 0, WINDOW_W, WINDOW_H);
    XFillRectangle(dpy, w, gc, 0, WINDOW_H-1, WINDOW_W, WINDOW_H);

    int x = 0;
    int y = 0;
    int direction = 0;
    int ascent = 0;
    int descent = 0;
    XCharStruct overall;

    XTextExtents (font, "Hello World!", getLettersCount("Hello World!"),
                  &direction, &ascent, &descent, &overall);

    x = 5; // (WINDOW_W - overall.width) / 2;
    y = 50; // WINDOW_H / 2 + (ascent - descent) / 2;

    int displayed_entries = 0;

    /* XClearWindow (dpy, w); */

    XDrawString (dpy, w, gc, x, 20, inserted_text, inserted_text_idx);
    for (int i = 0; i < NUMBER_OF_ENTRIES; ++i)
    {
        if (inserted_text[0] == '\0' || prefixMatch(entries[i], inserted_text))
        {
            XDrawString (dpy, w, gc, x, y + displayed_entries * (ascent - descent + 10),
                         entries[i], getLettersCount(entries[i]));

            displayed_entries++;
        }
    }
    XFlush(dpy);

#if 0
    printf(inserted_text);
    printf("\n");
#endif
}

int
main()
{
    inserted_text[0] = '\0';

    dpy = XOpenDisplay(NULL);
    assert(dpy);

    // Initialize color map.
    color_map = XDefaultColormap(dpy, 0);
    blackColor = BlackPixel(dpy, DefaultScreen(dpy));
    whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

    bg_color[0].red = bg_red * 256; bg_color[0].green = bg_green * 256; bg_color[0].blue = bg_blue * 256;
    bg_color[0].flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, color_map, bg_color);

    // MY:
    XSetWindowAttributes wa;
    wa.override_redirect = True;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

    // Create the window
    int scr = DefaultScreen(dpy);
    s = ScreenOfDisplay(dpy, scr);

    w = XCreateWindow(dpy, DefaultRootWindow(dpy),
                      (WidthOfScreen(s) - WINDOW_W) / 2,
                      (HeightOfScreen(s) - WINDOW_H) / 2,
                      WINDOW_W, WINDOW_H, 0, DefaultDepth(dpy, scr),
                      InputOutput, DefaultVisual(dpy, scr),
                      CWOverrideRedirect | CWBackPixmap | CWEventMask,
                      &wa);
    XClearWindow(dpy, w);

    // TODO: Copied from dmenu.
    struct timespec tim;
    tim.tv_sec = 0;
    tim.tv_nsec = 1000 * 1000 * 10;

    int grab_succeded = 0;
    for (int i = 0; i < 1000; ++i)
    {
        if (!XGrabKeyboard(dpy, DefaultRootWindow(dpy), True,
                           GrabModeAsync, GrabModeAsync, CurrentTime))
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

    set_up_font ();

    XFlush(dpy);

#if 1
    // a = characters[0][1];

    // TODO: PRobobly not the best way, but who cares?
    // int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    /* pixmap = XCreatePixmapFromBitmapData(dpy, w, (char *)a.pixels, a.w, a.h, */
    /* whiteColor, blackColor, depth); */

    // XFlush(dpy);

    /* XCreatePixmap(dpy, DefaultRootWindow(dpy), a.w, a.h, depth); */
    /* for (int i = 0; i < a.w; ++i) */
    /* for (int j = 0; j < a.h; ++j) */
    /* XDrawPoint(dpy, pixmap, gc, i, j); */
#endif

    redrawWindow();

    // Wait for the MapNotify event
    while(1)
    {
        int key_press = 0;

        XEvent e;
        XNextEvent(dpy, &e);
        // if (e.type == MapNotify)
        //    break;
        //else
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
                } break;

                case XK_BackSpace:
                {
                    if (inserted_text_idx)
                    {
                        inserted_text_idx--;
                        inserted_text[inserted_text_idx] = '\0';
                        redraw = 1;
                    }
                } break;

                case XK_Return:
                {
                    printf(inserted_text);
                    XUngrabKeyboard(dpy, CurrentTime);
                    exit(0);
                } break;

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

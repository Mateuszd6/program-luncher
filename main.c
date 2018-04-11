#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <X11/Xlib.h> // Every Xlib program must include this
#include <X11/Xutil.h>

#include <assert.h> // I include this to test return values the lazy way
#include <unistd.h> // So we got the profile for 10 seconds

#define WINDOW_W (640)
#define WINDOW_H (480)

int nanosleep(const struct timespec *req, struct timespec *rem);


#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define TTF_BUFFER_SIZE (1 << 25)
char unsigned ttf_buffer[TTF_BUFFER_SIZE];

typedef struct
{
    unsigned char *pixels;
    int w, h;
} BitmapInfo;
BitmapInfo characters['z' - 'a' + 1][2];

static void
stb_test()
{
    stbtt_fontinfo font;
    int w, h, i, j, fsize = 20;

    fread(ttf_buffer, 1, TTF_BUFFER_SIZE,
          fopen("/usr/share/fonts/TTF/DejaVuSans.ttf", "rb"));

    stbtt_InitFont(&font, ttf_buffer,
                   stbtt_GetFontOffsetForIndex(ttf_buffer, 0));

    for (int caps = 0; caps < 2; ++caps)
        for (int letter = 'a'; letter <= 'z'; ++letter)
        {
            unsigned char *bitmap = stbtt_GetCodepointBitmap(
                &font, 0, stbtt_ScaleForPixelHeight(&font, fsize),
                (caps ? letter + 'A' - 'a' : letter), &w, &h, 0,0);

            characters[letter - 'a'][caps] = (BitmapInfo){ bitmap, w, h };
        }

    for (int caps = 0; caps < 2; ++caps)
        for (int letter = 0; letter < 'z' - 'a' + 1; ++letter)
        {
            putchar('\n');
            putchar('\n');
            BitmapInfo bi = characters[letter][caps];
            for (j = 0; j < bi.h; ++j)
            {
                for (i =0 ; i < bi.w; ++i)
                    putchar(" .:ioVM@"[bi.pixels[j* bi.w +i]>>5]);
                putchar('\n');
            }
        }
}

Display *dpy;
XFontStruct *font;
GC gc;

static void
set_up_font ()
{
    const char * fontname = "-*-helvetica-*-r-*-*-18-*-*-*-*-*-*-*";
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

#define NUMBER_OF_ENTRIES (5)
const char *entries[NUMBER_OF_ENTRIES] =
{
    "Foo",
    "Bar",
    "FooBar",
    "Scoo",
    "Test"
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
int
getLettersCount(const char *text)
{
    for (int i = 0;; ++i)
        if (text[i] == '\0')
            return i;
}

int
main()
{
    stb_test();
    return 0;

    inserted_text[0] = '\0';

    dpy = XOpenDisplay(NULL);
    assert(dpy);

    int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
    int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

    // MY:
    XSetWindowAttributes wa;
    wa.override_redirect = True;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;

    // Create the window
#if 0
    Window w = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
                                   200, 100, 0, blackColor, blackColor);
#else
    int scr = DefaultScreen(dpy);
    Screen *s = ScreenOfDisplay(dpy, scr);

    Window w = XCreateWindow(dpy, DefaultRootWindow(dpy),
                             (WidthOfScreen(s) - WINDOW_W) / 2,
                             (HeightOfScreen(s) - WINDOW_H) / 2,
                             WINDOW_W, WINDOW_H, 0, DefaultDepth(dpy, scr),
                             InputOutput, DefaultVisual(dpy, scr),
                             CWOverrideRedirect | CWBackPixmap | CWEventMask,
                             &wa);

    // TODO: Is this really necessary?
    XClearWindow(dpy, w);
#endif

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

//   XUngrabKeyboard(dpy, CurrentTime);

    XChangeWindowAttributes(dpy, w, 0, &wa);

    // We want to get MapNotify events
    XSelectInput(dpy, w, StructureNotifyMask);

    // "Map" the window (that is, make it appear on the screen)
    XMapWindow(dpy, w);

    // Create a "Graphics Context"
    gc = XCreateGC(dpy, w, 0, NULL);

    // Tell the GC we draw using the white color
    XSetForeground(dpy, gc, blackColor);

    // TODO: Why is is not working?
    XSetBackground(dpy, gc, whiteColor);

    set_up_font ();

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
                    }
                } break;

                case XK_Return:
                {
                    printf(inserted_text);
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
            }


        }
        else if (e.type == ButtonPress)
        {
            XCloseDisplay(dpy);
            exit(2);
        }

        // Draw the line
        // XDrawLine(dpy, w, gc, 10, 60, 180, 20);
        // XDrawRectangle(dpy, w, gc, 0, 0, 200-1, 100-1);
        XSetForeground(dpy, gc, blackColor);
        XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, WINDOW_H);
        XSetForeground(dpy, gc, whiteColor);

        int x = 0;
        int y = 0;
        int direction = 0;
        int ascent = 0;
        int descent = 0;
        XCharStruct overall;

        /* Centre the text in the middle of the box. */
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

    XUngrabKeyboard(dpy, CurrentTime);
    return 0;
}

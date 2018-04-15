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
BitmapInfo characters['z'-'a'+1][2];

// TODO: testcode:
static unsigned char alien_bits[] =
{
    0x13, 0xc8, 0xa2, 0x45, 0xc3, 0xc3, 0xe2, 0x47, 0xf3, 0xcf, 0xbe, 0x7d,
    0xf8, 0x1f, 0xf8, 0x1f, 0xf8, 0x1f, 0x3f, 0xfc, 0xf2, 0x4f, 0xe3, 0xc7,
    0xc2, 0x43, 0x83, 0xc1, 0x02, 0x40, 0x03, 0xc0
};


static BitmapInfo a; // TODO: Dont make itt global, fix once cleanup.

static void
stb_test()
{
    stbtt_fontinfo font;
    int w, h, i, j, fsize = 256;

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

unsigned char temp_bitmap[1 << 20];
stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs

static void
stb_test2()
{
    fread(ttf_buffer, 1, 1 << 20,
          fopen("/usr/share/fonts/TTF/DejaVuSans.ttf", "rb"));

    int bake_font_bitmap_res;
    bake_font_bitmap_res = stbtt_BakeFontBitmap(ttf_buffer, 0, 50, temp_bitmap,
                                                1024, 1024, 'a', 'z'-'a'+1, cdata);

    printf("Baking bitmap result: %d\n", bake_font_bitmap_res);
}

Display *dpy;
Window w;
Screen *s;
XFontStruct *font;
GC gc;
int whiteColor, blackColor;
Pixmap pixmap;

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
    // Draw the line
    // XDrawLine(dpy, w, gc, 10, 60, 180, 20);
    // XDrawRectangle(dpy, w, gc, 0, 0, 200-1, 100-1);
    // TODO: Dont draw the background all the time.
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
    // TODO: Super-creepy so far! I would rather not use it, because it is
    // more likely to break something than work... For now.
#if 1

    XSetBackground(dpy, gc, whiteColor);
    XSetForeground(dpy, gc, blackColor);

    // NOTE: USe one of these two??
    // XCopyArea(dpy, pixmap, w, gc, 0, 0,
    //          a.w, a.h, 100, 50);

    /* XCopyPlane(dpy, pixmap, w, gc, 0, 0, */
    /* a.w, a.h, a.w, a.h, 1); */

    // TODO: Copied from dmenu.
    // NOTE: Copied Area seeps not to appear so we give it some time.

#if 0
    struct timespec tim;
    tim.tv_sec = 0;
    tim.tv_nsec = 1000 * 1000 * 10;
    nanosleep(&tim, NULL);
#endif

#endif
    XFlush(dpy);


#if 0
    printf(inserted_text);
    printf("\n");
#endif
}

int
main()
{
    // stb test
    stb_test2();

    for (int i = 0; i < 96; ++i)
    {
        stbtt_bakedchar character_info = cdata[i];
        printf("%d: (%d;%d)x(%d;%d), %f, %f, %f\n", i,
               character_info.x0, character_info.y0,
               character_info.x1, character_info.y1,
               character_info.xoff, character_info.yoff,
               character_info.xadvance);
    }

    // (29;1)x(41;17)
    // (1;1)x(14;17)
    int x0, y0, x1, y1;

#if 1
    stbtt_bakedchar c = cdata[3];
    x0 = c.x0; y0 = c.y0;
    x1 = c.x1; y1 = c.y1;
#else
    x0 = 1; y0 = 1;
    x1 = 14; y1 = 17;
#endif

    for (int j = y0; j < y1; ++j)
    {
        for (int i = x0; i < x1; ++i)
            putchar(" .:ioVM@"[temp_bitmap[j * 1024 + i] >> 5]);
            /* printf("%d ", temp_bitmap[j * 1024 + i]); */
        putchar('\n');
    }
    //endof stb test.

    inserted_text[0] = '\0';

    dpy = XOpenDisplay(NULL);
    assert(dpy);

    blackColor = BlackPixel(dpy, DefaultScreen(dpy));
    whiteColor = WhitePixel(dpy, DefaultScreen(dpy));

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

//   XUngrabKeyboard(dpy, CurrentTime);

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

    XSetForeground(dpy, gc, blackColor);
    XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, WINDOW_H);

    set_up_font ();

#if 1
    // a = characters[0][1];

    XSetForeground(dpy, gc, whiteColor);
    // TODO: PRobobly not the best way, but who cares?
    int depth = DefaultDepth(dpy, DefaultScreen(dpy));
    /* pixmap = XCreatePixmapFromBitmapData(dpy, w, (char *)a.pixels, a.w, a.h, */
    /* whiteColor, blackColor, depth); */

    stbtt_bakedchar character_info = cdata[0];
    pixmap = XCreatePixmap(dpy, w,
                           character_info.y1-character_info.y0,
                           character_info.x1-character_info.x0,
                           depth);

    // TODO: set pixels of the bitmap!

    XCopyArea(dpy, pixmap,  w, gc, 0, 0,
              character_info.y1-character_info.y0,
              character_info.x1-character_info.x0,
              40, 40);

    XFlush(dpy);

    sleep(2);

    /* XCreatePixmap(dpy, DefaultRootWindow(dpy), a.w, a.h, depth); */
    /* for (int i = 0; i < a.w; ++i) */
    /* for (int j = 0; j < a.h; ++j) */
    /* XDrawPoint(dpy, pixmap, gc, i, j); */
#endif

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

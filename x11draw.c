#include "x11draw.h"
#include "util.h"

#define WINDOW_W (640)
#define WINDOW_H (256)
#define OFFSET_X (5)
#define OFFSET_Y (20)

static Display *dpy; // TODO: make static
static Window w;
static Screen *s;
static XFontStruct *font;
static GC gc;

static XColor bg_color[2];
static XColor font_color;
static XColor shaded_font_color;
static XColor lines_color;
static XColor selected_field_color;
static XColor selected_font_color;

static Colormap color_map;

// TODO: Add ability to specify colors by the user.
static ColorData bg_color_data = {0x27, 0x28, 0x22}; // {0x1e, 0x1f, 0x1c};
static ColorData lines_color_data = {0x02, 0x02, 0x02};
static ColorData font_color_data = {0xCF, 0xCF, 0xCF};
static ColorData selected_field_color_data = {0x14, 0x33, 0x66}; // {0x47, 0x19, 0x8D};
static ColorData selected_font_color_data = {0xCF, 0xCF, 0xCF};

int prompt_len = 0;

// TODO: Drawing options?
static int draw_lines = 1;

// TODO:?
static int fields_drawned = 0;

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

static void AllocateXColorFromColorData(XColor *xcolor, const ColorData data)
{
    xcolor->red = data.red * 256;
    xcolor->green = data.green * 256;
    xcolor->blue = data.blue * 256;
    xcolor->flags = DoRed | DoGreen | DoBlue;
    XAllocColor(dpy, color_map, xcolor);
}

static void KeyboardInit()
{
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
        // TODO: Handle this case in a better way!
        assert(0);
    }

    // NOTE: For testing stuff that does not work with keyboard nor needs it.
#if 0
    XUngrabKeyboard(dpy, CurrentTime);
#endif
}

void DrawBackground()
{
    // Draw the background.
    XSetForeground(dpy, gc, bg_color[0].pixel);
    XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, WINDOW_H);

    for (int i = 0; i < fields_drawned; ++i)
    {
        XSetForeground(dpy, gc,
                       (i & 1 ? bg_color[0].pixel : bg_color[1].pixel));
        XFillRectangle(dpy, w, gc, 0,
                       OFFSET_Y +
                           i * (font_info.ascent - font_info.descent + 10) + 5,
                       WINDOW_W, font_info.ascent - font_info.descent + 10);
    }
}

void DrawMarginLines()
{
    // Draw the margins:
    if (draw_lines)
    {
        XSetForeground(dpy, gc, lines_color.pixel);
        XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, 1);
        XFillRectangle(dpy, w, gc, 0, 0, 1, WINDOW_H);
        XFillRectangle(dpy, w, gc, WINDOW_W - 1, 0, WINDOW_W, WINDOW_H);
        XFillRectangle(dpy, w, gc, 0, WINDOW_H - 1, WINDOW_W, WINDOW_H);

        // TODO: If displaying prompt:
        // TODO: INVESTIGATE WHY THE FIRST LINE IS HIGHTER THAN THE REST AND BY
        // HOW MUCH IT IS!!
        if (prompt_len)
            XFillRectangle(dpy, w, gc, prompt_len, 0, 1,
                           (font_info.ascent - font_info.descent + 13));

        for (int i = 0; i < fields_drawned; ++i)
            XFillRectangle(
                dpy, w, gc, 0,
                OFFSET_Y + i * (font_info.ascent - font_info.descent + 10) + 5,
                WINDOW_W, 1);
    }
}

void DrawInertedTextAndPrompt(const char *text)
{
    int ascend, descend, direction;
    XCharStruct overall;

    if (prompt_len)
    {
        XSetForeground(dpy, gc, selected_field_color.pixel);
        XTextExtents(font, prompt, GetLettersCount(prompt),
                     &direction, &ascend, &descend, &overall);
        XFillRectangle(dpy, w, gc,
                       0, 0, prompt_len, // Make some offset from the both sides!
                       font_info.ascent - font_info.descent + 13);

        XSetForeground(dpy, gc, selected_font_color.pixel);
        XDrawString(dpy, w, gc, OFFSET_X, OFFSET_Y, prompt, GetLettersCount(prompt));
    }

    XSetForeground(dpy, gc, font_color.pixel);
    XTextExtents(font, text, GetLettersCount(text),
                 &direction, &ascend, &descend, &overall);

    XDrawString(dpy, w, gc, prompt_len + OFFSET_X, OFFSET_Y,
                text, GetLettersCount(text));

    // Draw cursor:
    // TODO: There are some numbers that wont change with a window, so work on
    // it for a while!
    XSetForeground(dpy, gc, shaded_font_color.pixel);
    XFillRectangle(dpy, w, gc,
                   prompt_len + OFFSET_X + overall.width + 2,
                   OFFSET_Y - (ascend - descend + 3),
                   2, ascend - descend + 6);
}

// Return 1 if there is no space for another entry
int DrawEntry(const char *text, const int entry_idx, const int selected)
{
    XSetForeground(dpy, gc, font_color.pixel);
    if (selected)
    {
        XSetForeground(dpy, gc, selected_field_color.pixel);
        XFillRectangle(
            dpy, w, gc, 0,
            OFFSET_Y + entry_idx * (font_info.ascent - font_info.descent + 10) +
                5,
            WINDOW_W, font_info.ascent - font_info.descent + 10);

        XSetForeground(dpy, gc, selected_font_color.pixel);
    }

    XDrawString(dpy, w, gc, OFFSET_X,
                OFFSET_Y + (entry_idx + 1) *
                               (font_info.ascent - font_info.descent + 10),
                text, GetLettersCount(text));

    return (entry_idx + 1 > fields_drawned);
}

void DrawAndKeyboardInit()
{
    dpy = XOpenDisplay(NULL);
    assert(dpy);

    KeyboardInit();

    // Initialize color map.
    color_map = XDefaultColormap(dpy, 0);

    AllocateXColorFromColorData(bg_color, bg_color_data);
    // Compute the second BG color from the first one.
    AllocateXColorFromColorData(bg_color + 1,
                                (ColorData){MAX(bg_color_data.red - 0x08, 0),
                                      MAX(bg_color_data.green - 0x08, 0),
                                      MAX(bg_color_data.blue - 0x08, 0)});
    AllocateXColorFromColorData(&font_color, font_color_data);
    // Do similar computation for the color of the cursor.
    AllocateXColorFromColorData(&shaded_font_color,
                                (ColorData){MAX(font_color_data.red - 0x10, 0),
                                        MAX(font_color_data.green - 0x10, 0),
                                        MAX(font_color_data.blue - 0x10, 0)});
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

    w = XCreateWindow(dpy, DefaultRootWindow(dpy),
                      (WidthOfScreen(s) - WINDOW_W) / 2,
                      -1, //(HeightOfScreen(s) - WINDOW_H) / 2,
                      WINDOW_W, WINDOW_H, 0, DefaultDepth(dpy, scr),
                      InputOutput, DefaultVisual(dpy, scr),
                      CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

    XClearWindow(dpy, w);

    XChangeWindowAttributes(dpy, w, 0, &wa);

    // We want to get MapNotify events
    XSelectInput(dpy, w, StructureNotifyMask);

    // "Map" the window (that is, make it appear on the screen)
    XMapWindow(dpy, w);

    // Create a "Graphics Context"
    gc = XCreateGC(dpy, w, 0, NULL);

    // I guess XParseColor will work here
    XSetForeground(dpy, gc, bg_color[0].pixel);

    XFillRectangle(dpy, w, gc, 0, 0, WINDOW_W, WINDOW_H);

    SetUpFont();

    // Calculate font-related stuff.
    XTextExtents(font, "Heljo World!", sizeof("Heljo World!"),
                 &font_info.direction, &font_info.ascent, &font_info.descent,
                 &font_info.overall);

    fields_drawned =
        (int)(WINDOW_H / (font_info.ascent - font_info.descent + 10)) + 1;

    if (prompt[0] != '\0')
    {
        XCharStruct prompt_overall;
        int ascend, descend, direction;

        XTextExtents(font, prompt, GetLettersCount(prompt),
                     &direction, &ascend, &descend, &prompt_overall);
        prompt_len = prompt_overall.width + 2*OFFSET_X;
    }
    else
        prompt_len = 0;

    // TODO: DO we need flush here?
    XFlush(dpy);
}

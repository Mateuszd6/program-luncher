#ifndef X11_DRAW_H
#define X11_DRAW_H

extern Display *dpy;

void DrawAndKeyboardInit();

void RedrawWindow();

void EventLoop();

#endif

#ifndef X11_DRAW_H
#define X11_DRAW_H

extern Display *dpy;

void RedrawWindow();

void DrawAndKeyboardInit();

void EventLoop();

#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

Display *dpy;
Window root;

int main(void) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Failed to open Display\n");
        exit(1);
    }

    root = DefaultRootWindow(dpy);

    // select events we want to listen to on the root window
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);

    printf("TerminateX started, waiting for those windows..\n");

    for (;;) {
        XEvent ev;
        XNextEvent(dpy, &ev); 
        if (ev.type == MapRequest) {
            Window w = ev.xmaprequest.window;
            XMoveResizeWindow(dpy, w, 50, 50, 400, 300);
            XMapRaised(dpy, w);
            printf("Mapped new window %lu\n", ev.xmaprequest.window);
        }
    }
    XCloseDisplay(dpy);
    return 0;
}

#include "xpm/REI2.xpm"
#include "xpm/REI_EYES.xpm"

#include "xpm/MISATO.xpm"
#include "xpm/MISATO_EYES.xpm"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>    // for getenv
#include <unistd.h>    // for fork
#include <libgen.h>    // for basename
#include <time.h>      // for timezone
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

//#include <valgrind/callgrind.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))


Display* dpy;
int screen;
bool blink;
Window win;
Window root;
Window active;

Window root, parent, *children = NULL;
Atom active_property;
XWindowAttributes attrib;
XpmAttributes girl_attrib;
XpmAttributes eyes_attrib;
int width,height;
int girl_width;
int eye_offset;
int height_offset;

GC gc;
Pixmap girl;
Pixmap eyes;
Pixmap mask;
Pixmap blank;

char* *GIRL;
char* *EYES;

XEvent event;

int XWinSize = 64;
int YWinSize = 128;

int XOffset = 0;

typedef struct Hints
{
    unsigned long   flags;
    unsigned long   functions;
    unsigned long   decorations;
    long            inputMode;
    unsigned long   status;
} Hints;

int handler(Display * d, XErrorEvent * e) {
	printf("Shit!\n");
	return(0);
}

int ExposeWindow() {
	XEvent exposee;
	//memset(&exposee, 0, sizeof(exposee));
	XSendEvent(dpy, win, false, ExposureMask, &exposee);
	XFlush(dpy);
	return(0);
}

int Redraw(bool expose) {
	if (expose) {
		ExposeWindow();
	}
	XCopyArea(dpy, girl, win, gc, XOffset, 0, width, height, 0, 0);
	XShapeCombineMask(dpy,win,ShapeBounding,-XOffset,0,mask,ShapeSet);
	return(0);
}

int Blink() {
	XCopyArea(dpy, eyes, win, gc, XOffset, 0, width, eye_offset, 0, 24);
	ExposeWindow();
	return(0);
}

int ShiftCostumes() {
	XOffset += girl_width;
	if (XOffset == width)
		XOffset = 0;
	return(0);
}

void *TimerThread() {
	srand(time(NULL));
	int r = 1;
	while(1){
		usleep(100000); // 0.1 seconds
		if (blink){
			blink = false;
			//printf("No more blink");
			Redraw(false);
			XFlush(dpy);
		}
		//XOffset = 128;
		//Redraw(true);
//		sleep(1);

		r = rand();
		if (r%20==0) {
			printf("Blink\n");
			blink = true;
			//XOffset = 0;
			Blink();
			//Redraw(true);
		}
	}
}

void *GirlThread() {
	int x, y;
	int oldx, oldy;
	bool firstrun = true;

	XWindowAttributes xwa;
	XWindowAttributes active_xwa;
	Window child;
	unsigned int num_children;
	XClassHint class;
	while (True) {
		XNextEvent(dpy, &event);
		//printf("Event %x\n", event.type);
		if (event.type == Expose && !blink) { 
			Redraw(false);
		} else if ( event.type == ButtonPress) { // 1 = left click, 2 = middle click, 3 = right click
			switch (event.xbutton.button) {
				case 1:
					ShiftCostumes();
					Redraw(true);
					break;
				case 2:
					return(0); // Exit
					break;
			}
		} else if ( event.type == PropertyNotify || event.type == ConfigureNotify) {
			Atom type_return;
			int format_return;
			unsigned long nitems_return;
			unsigned long bytes_left;
			unsigned char *data;
			XGetWindowProperty(dpy,root,active_property, 0,1,false,XA_WINDOW,&type_return,&format_return,&nitems_return,&bytes_left,&data); // Get the active window
			//printf("Data %x\n", data);
			if (type_return == XA_WINDOW) {
/*
				if (((Window*) data)[0]!= active)
					XSelectInput(dpy, active, NoEventMask);
*/
				active = ((Window*) data)[0];
				if (active != win){
					//printf("Event Type %x\n", event.
					XSelectInput(dpy, active, StructureNotifyMask);
					
					//XGetClassHint(dpy, active, &class);
					//printf("%s\n", class.res_name);
					XQueryTree(dpy, active, &root, &parent, &children, &num_children);
					XFree(children);
					children = NULL; // Avoid double free
					oldx = x;
					oldy = y;
					XTranslateCoordinates( dpy, parent, root, 0, 0, &x, &y, &child );
					XGetWindowAttributes( dpy, win, &xwa );
					XGetWindowAttributes( dpy, active, &active_xwa );
					x = x - xwa.x;
					y = y - xwa.y;
					//printf( "X %d Y %d Width %d Height %d\n", x, y, active_xwa.width, active_xwa.height);
					printf("OldX %d OldY %d NewX %d NewY %d\n",oldx,oldy,x,y);
					
					//XSetTransientForHint(dpy,win,active);
					if (x != oldx || y != oldy || firstrun) {
						printf("Moved\n");
						XMoveWindow(dpy, win, MAX(x+active_xwa.width-128,x), y-height_offset);
						firstrun = false;
						XFlush(dpy);
					}
					//XRaiseWindow(dpy, win);
					XFree(data);
					data = NULL; // Avoid double free
				}
			} else {
				printf("Nope\n");
			}
		}
    }
}

#define _NET_WM_STATE_REMOVE        0    // remove/unset property
#define _NET_WM_STATE_ADD           1    // add/set property
#define _NET_WM_STATE_TOGGLE        2    // toggle property

void set_window_front(Display *disp, Window wind)
{
	Atom wm_state, wm_state_above;
	XEvent event;

	if ((wm_state = XInternAtom(disp, "_NET_WM_STATE", False)) != None)
	{
		if ((wm_state_above = XInternAtom(disp, "_NET_WM_STATE_ABOVE", False))
			!= None)
		{
			/* sending a ClientMessage */
			event.xclient.type = ClientMessage;

			/* value unimportant in this case */
			event.xclient.serial = 0;

			/* coming from a SendEvent request, so True */
			event.xclient.send_event = True;

			/* the event originates from disp */
			event.xclient.display = disp;

			/* the window whose state will be modified */
			event.xclient.window = wind;

			/* the component Atom being modified in the window */
			event.xclient.message_type = wm_state;

			/* specifies that data.l will be used */
			event.xclient.format = 32;

			/* 1 is _NET_WM_STATE_ADD */
			event.xclient.data.l[0] = 1;

			/* the atom being added */
			event.xclient.data.l[1] = wm_state_above;

			/* unused */
			event.xclient.data.l[2] = 0;
			event.xclient.data.l[3] = 0;
			event.xclient.data.l[4] = 0;

			/* actually send the event */
			XSendEvent(disp, DefaultRootWindow(disp), False,
				SubstructureRedirectMask | SubstructureNotifyMask, &event);
		}
	}
}

int usage() {
	printf("Usage: awcharse [rei|misato]\n");
	return(0);
}

int main(int argc, char** argv) {
	printf("argc %d\n", argc);
	if (argc != 2) {
		usage();
		return(1);
	} else {
		if (strcmp(argv[1], "rei") == 0) {
			GIRL = REI;
			EYES = REI_EYES;
			girl_width = 64;
			eye_offset = 18;
			height_offset = 95;
		} else if (strcmp(argv[1], "misato") == 0) {
			GIRL = MISATO;
			EYES = MISATO_EYES;
			girl_width = 80;
			eye_offset = 24;
			height_offset = 90;
		} else {
			usage();
			return(1);
		}
	}

	XInitThreads();
	XSetErrorHandler(handler);
    if ((dpy = XOpenDisplay(getenv("DISPLAY"))) == 0)
    {
        fprintf(stderr, "Can't open display: %s\n", getenv("DISPLAY"));
        exit(1);
    }
    screen = DefaultScreen(dpy);
	active_property = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False); 
	printf("Hello, your display is %s\n", getenv("DISPLAY"));

	
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                              0, 0, XWinSize, YWinSize, 0, 0, 0);
	if (!win)
    {
        fprintf(stderr, "Can't create window\n");
        exit(1);
    }

	root = RootWindow(dpy, screen);

	XStoreName(dpy,win,"Evangelion window sitter for X11");
	XGetWindowAttributes(dpy,win,&attrib);

	Hints hints;
	Atom property;
	hints.flags = 2;
	hints.decorations = 0;
	property = XInternAtom(dpy, "_MOTIF_WM_HINTS", true);
	XChangeProperty(dpy,win,property,property,32,PropModeReplace,(unsigned char *)&hints,5);

	XpmCreatePixmapFromData(dpy, win, GIRL, &girl, &mask, &girl_attrib);
	XpmCreatePixmapFromData(dpy, win, EYES, &eyes, NULL, &eyes_attrib);
	width  = girl_attrib.width;
	height = girl_attrib.height;
	printf("Width = %d Height = %d\n",width,height);
	XResizeWindow(dpy,win,girl_width,height);

	XSizeHints *sizehints;
	sizehints = XAllocSizeHints();
	sizehints->flags = PMinSize|PMaxSize;
	sizehints->min_width  = girl_width;
	sizehints->min_height = height;
	sizehints->max_width  = girl_width;
	sizehints->max_height = height;

    XGCValues gcValues;
    gcValues.foreground = WhitePixel(dpy, screen);
    gcValues.background = BlackPixel(dpy, screen);

    //gc = XCreateGC(dpy, win, GCForeground | GCBackground, &gcValues);
	gc = XCreateGC(dpy,win,0,NULL);

//	int depth = DefaultDepthOfScreen(screen);
//	XSetWindowBackgroundPixmap(dpy,win,bitmap);
//	XShapeCombineMask(dpy,win,ShapeBounding,0,0,mask,ShapeSet);

//	XCopyArea(dpy, bitmap, win, gc, 0, 0, 384, 128, 0, 0);
	
    XSelectInput(dpy, win, ExposureMask|KeyPressMask|ButtonPress|StructureNotifyMask);
	XSelectInput(dpy, root, PropertyChangeMask);
	XMapWindow(dpy, win);
	
	XSetWMNormalHints(dpy,win,sizehints);
	set_window_front(dpy,win);
	
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, TimerThread, NULL);

	pthread_t girl_thread;
    pthread_create(&girl_thread, NULL, GirlThread, NULL);
	
	pthread_join(girl_thread, NULL);

	printf("Bye\n");
	return(0);
}

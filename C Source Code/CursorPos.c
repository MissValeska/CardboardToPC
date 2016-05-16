/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CursorPos.c
 * Author: valeska
 *
 * Created on May 14, 2016, 12:30 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h> // Every Xlib program must include this
#include <assert.h>   // I include this to test return values the lazy way
#include <unistd.h>   // So we got the profile for 10 seconds

void MoveCursorPos(int x, int y){
Display *dpy;
//Window root_window;

dpy = XOpenDisplay(0);
assert(dpy);
//root_window = XRootWindow(dpy, 0);
//XSelectInput(dpy, root_window, KeyReleaseMask);
XWarpPointer(dpy, None, None, 0, 0, 0, 0, x, y);
XFlush(dpy); // Flushes the output buffer, therefore updates the cursor's position. Thanks to Achernar.
}
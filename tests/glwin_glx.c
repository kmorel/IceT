/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

#ifndef _WIN32

/* 
 * This code opens up a GL window in X
 * Id
 *
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>   /* XVisualInfo  */
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>


static Bool      WaitForMapNotify( Display *d, XEvent *e, char *arg );

static Window           glwin;
static Display         *dpy;

int wincreat( int x, int y, int width, int height, char *title)
{ 
  XSetWindowAttributes   swa; 
  Colormap               cmap;
  XVisualInfo           *vi;
  int                    dummy;
  GLXContext             glcx;
  XEvent                 event;
  int                    attributes[] = { GLX_RGBA,
                                          GLX_DEPTH_SIZE, 16, 
                                          GLX_RED_SIZE, 1, 
                                          GLX_GREEN_SIZE, 1, 
                                          GLX_BLUE_SIZE, 1,
                                          GLX_DOUBLEBUFFER,
                                          None }; 

    if( !(dpy = XOpenDisplay( NULL )) )    /* defaults to $DISPLAY */
  {
      fprintf( stderr, "Unable to open display.\n" );
      exit( 1 );

  } else {

    /*printf( "Connected to display... %s (%s).\n", dpy->display_name, dpy->vendor );*/

  }   /* end if( ) */

  if( !glXQueryExtension( dpy, &dummy, &dummy ) )
  {
      fprintf( stderr, "Unable to query GLX extensions.\n" );
      exit( 1 );

  }   /* end if( ) */

  if( !(vi = glXChooseVisual( dpy, DefaultScreen( dpy ), attributes )) )
  {
      fprintf( stderr, "Unable get a visual.\n" );
      exit( 1 );

  }   /* end if( ) */

  if( vi->class != TrueColor )
  {
      fprintf( stderr, "Need TrueColor class.\n" );
      exit( 1 );
  }

  if( !(glcx = glXCreateContext( dpy, vi, None, GL_TRUE )) )
  {
      fprintf( stderr, "Unable create a GL context.\n" );
      exit( 1 );

  }   /* end if( ) */

  cmap = XCreateColormap( dpy, RootWindow( dpy, vi->screen ),
                                     vi->visual, AllocNone );

  swa.colormap                  = cmap;
  swa.border_pixel              = 0;
  swa.event_mask                = ExposureMask
                                | KeyPressMask
                                | StructureNotifyMask;

  glwin = XCreateWindow(        dpy,
                                RootWindow( dpy, vi->screen ),
                                x,
                                y,
                                width,
                                height,
                                0,
                                vi->depth,
                                InputOutput,
                                vi->visual,
                                CWBorderPixel   |
                                CWColormap      |
                                CWEventMask,
                                &swa
                        );

/* Make a clear cursor so it looks like we have none. */
  {
      Pixmap pixmap;
      Cursor cursor;
      XColor color;
      char clear_bits[32];

      memset(clear_bits, 0, sizeof(clear_bits));

      pixmap = XCreatePixmapFromBitmapData(dpy, glwin, clear_bits,
                                           16, 16, 1, 0, 1);
      cursor = XCreatePixmapCursor(dpy, pixmap, pixmap, &color, &color, 8, 8);
      XDefineCursor(dpy, glwin, cursor);

      XFreePixmap(dpy, pixmap);
  }


  XSetStandardProperties( dpy, glwin, title, title, None, NULL, 0, NULL );

  if( !glXMakeCurrent( dpy, glwin, glcx ) )
  {
      fprintf( stderr, "Failed to make the GL context current.\n" );
      exit( 1 );
  }

  XMapWindow( dpy, glwin );
  XIfEvent( dpy, &event, WaitForMapNotify, (char *)glwin );

  return( 1 );

} /* end int APIENTRY pglc_wincreat( ) */
        
void resize( GLsizei width, GLsizei height ) 
{ 

  GLfloat       aspect = (GLfloat) width / height; 

  glViewport( 0, 0, width, height ); 
  glMatrixMode( GL_PROJECTION ); 
  glLoadIdentity( ); 
  gluPerspective( 45.0, aspect, .1, 10000 ); 

  glMatrixMode( GL_MODELVIEW ); 

} /* end void resize( ) */    

static Bool WaitForMapNotify(Display *foo, XEvent *e, char *arg) 
{ 
  /* To remove warning */
  (void)foo;

  if( (e->type == MapNotify) && (e->xmap.window == (Window)arg) )
  { 
      return( GL_TRUE );

  } else  {

      return( GL_FALSE );
  }

} /* end static Bool WaitForMapNotify( ) */

void swap_buffers(void)
{
    glXSwapBuffers(dpy, glwin);
}

#endif /* WIN32 */

//
// "$Id: Fl_Cairo.cxx 10027 2013-12-11 06:33:14Z fabien $"
//
// Main header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2010 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     http://www.fltk.org/COPYING.php
//
// Please report all bugs and problems on the following page:
//
//     http://www.fltk.org/str.php
//

#include <config.h>

#ifdef FLTK_HAVE_CAIRO
#include <FL/Fl.H>
#include <FL/x.H>
#include <FL/Fl_Window.H>
#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

// static Fl module initialization :
Fl_Cairo_State Fl::cairo_state_;	///< contains all necesary info for current cairo context mapping


// Fl cairo features implementation

// Fl_Cairo_State class impl

void  Fl_Cairo_State::autolink(bool b)  {
#ifdef FLTK_USE_CAIRO
  autolink_ = b;
#else
  Fl::fatal("In Fl::autolink(bool) : Cairo autolink() feature is only "
	   "available with the enable-cairoext configure option, now quitting.");
#endif
}

/** 
    Provides a corresponding cairo context for window \a wi.
    This is needed in a draw() override if Fl::cairo_autolink_context() 
    returns false, which is the default.
    The cairo_context() does not need to be freed as it is freed every time 
    a new cairo context is created. When the program terminates,
    a call to Fl::cairo_make_current(0) will destroy any residual context.
    \note A new cairo context is not always re-created when this method
    is used. In particular, if the current graphical context and the current 
    window didn't change between two calls, the previous gc is internally kept,
    thus optimizing the drawing performances.
    Also, after this call, Fl::cairo_gc() is adequately updated with this 
    cairo  context.
    \note Only available when configure has the --enable-cairo option
    \return the valid cairo_t* cairo context associated to this window.
*/
cairo_t * Fl::cairo_make_current(Fl_Window* wi) {
    if (!wi) return NULL; // Precondition
    
    if (fl_gc==0) { // means remove current cc
	Fl::cairo_cc(0); // destroy any previous cc
	cairo_state_.window(0);
	return 0;
    }

    // don't re-create a context if it's the same gc/window couple
    if (fl_gc==Fl::cairo_state_.gc() && fl_xid(wi) == (Window) Fl::cairo_state_.window())
	return Fl::cairo_cc();

    cairo_state_.window(wi);

#if defined(USE_X11)
    return Fl::cairo_make_current(0, wi->w(), wi->h());
#else
    return Fl::cairo_make_current(fl_gc, wi->w(), wi->h());
#endif
}

/* 
    Creates transparently a cairo_surface_t object.
    gc is an HDC context in  WIN32, a CGContext* in Quartz, a display on X11
 */

static cairo_surface_t * cairo_create_surface(void * gc, int W, int H) {
# if defined(USE_X11)
    return cairo_xlib_surface_create(fl_display, fl_window, fl_visual->visual, W, H);
# elif   defined(WIN32)
    return cairo_win32_surface_create((HDC) gc);
# elif defined(__APPLE_QUARTZ__)
    return cairo_quartz_surface_create_for_cg_context((CGContext*) gc, W, H);
# else
#  error Cairo is not supported under this platform.
# endif
}

/** 
  Creates a cairo context from a \a gc only, get its window size or offscreen size if fl_window is null.
   \note Only available when configure has the --enable-cairo option
*/
cairo_t * Fl::cairo_make_current(void *gc) {
    int W=0,H=0;
#if defined(USE_X11)
    //FIXME X11 get W,H
    // gc will be the window handle here
# warning FIXME get W,H for cairo_make_current(void*)
#elif defined(__APPLE_QUARTZ__) 
    if (fl_window) {
#if __LP64__
      W = Fl_Window::current()->w();
      H = Fl_Window::current()->h();
#else
      Rect portRect; 
      GetPortBounds(GetWindowPort( Fl_X::i(Fl_Window::current())->window_ref() ), &portRect);
      W = portRect.right-portRect.left;
      H = portRect.bottom-portRect.top;
#endif
    } 
    else {
      W = CGBitmapContextGetHeight(fl_gc);
      H = CGBitmapContextGetHeight(fl_gc);
    }
#elif defined(WIN32)
    // we don't need any W,H for WIN32
#else
# error Cairo is not supported under this platform.
#endif
    if (!gc) {
	Fl::cairo_cc(0);
	cairo_state_.gc(0); // keep track for next time
	return 0;
    }
    if (gc==Fl::cairo_state_.gc() && 
	fl_window== (Window) Fl::cairo_state_.window() && 
	cairo_state_.cc()!=0)
	return Fl::cairo_cc();
    cairo_state_.gc(fl_gc); // keep track for next time
    cairo_surface_t * s = cairo_create_surface(gc, W, H);
    cairo_t * c = cairo_create(s);
    cairo_surface_destroy(s);
    cairo_state_.cc(c);
    return c;
}

/** 
   Creates a cairo context from a \a gc and its size 
   \note Only available when configure has the --enable-cairo option
*/
cairo_t * Fl::cairo_make_current(void *gc, int W, int H) {
    if (gc==Fl::cairo_state_.gc() && 
	fl_window== (Window) Fl::cairo_state_.window() &&
	cairo_state_.cc()!=0) // no need to create a cc, just return that one
	return cairo_state_.cc(); 

    // we need to (re-)create a fresh cc ...
    cairo_state_.gc(gc); // keep track for next time
    cairo_surface_t * s = cairo_create_surface(gc, W, H);
    cairo_t * c = cairo_create(s);
    cairo_state_.cc(c); //  and purge any previously owned context
    cairo_surface_destroy(s);
    return c;
}
#else
// just don't leave the libfltk_cairo lib empty to avoid warnings
#include <FL/Fl_Export.H>
FL_EXPORT int fltk_cairo_dummy() { return 1;}
#endif // FLTK_HAVE_CAIRO

//
// End of "$Id: Fl_Cairo.cxx 10027 2013-12-11 06:33:14Z fabien $" .
//

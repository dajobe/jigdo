/* $Id$ -*- C++ -*-

  This code was taken from glibwww2
  <http://cvs.gnome.org/lxr/source/glibwww2/>, main author: James
  Henstdridge <james@daa.com.au>, distributable under GPL, v2 or
  later.

*/
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef _GLIBWWW_H_
#define _GLIBWWW_H_

#include <config.h>
#include <glib.h>

/* [RA] public: */
void glibwww_parse_proxy_env();

/* Defined here so we don't have to include any libwww headers */
typedef struct _HTRequest GWWWRequest;

/* If status < 0, an error occured */
typedef void (*GWWWLoadToFileFunc) (const gchar *url, const gchar *file,
                                    int status, gpointer user_data);
typedef void (*GWWWLoadToMemFunc) (const gchar *url, const gchar *buffer,
                                   int size, int status, gpointer user_data);

/* Initialise enough of libwww for doing http/ftp downloads with
 * authentication, redirection and proxy support.
 */
void glibwww_init    (const gchar *appName, const gchar *appVersion);
void glibwww_cleanup (void); /* not necessary -- registered with g_atexit() */

/* register the GUI dialogs for glibwww.  This will take care of all the
 * authentication and progress bar stuff for the application. */
void glibwww_register_gnome_dialogs (void);

/* Setup proxies as needed -- use the http://proxyhost:port/ notation */
void glibwww_add_proxy   (const gchar *protocol, const gchar *proxy);
void glibwww_add_noproxy (const gchar *host);

/* Load a url to a file or to memory.  The callback will be invoked
 * exactly once. */
GWWWRequest *glibwww_load_to_file (const gchar *url, const gchar *file,
                                   GWWWLoadToFileFunc callback,
                                   gpointer user_data);

GWWWRequest *glibwww_load_to_mem (const gchar *url,
                                  GWWWLoadToMemFunc callback,
                                  gpointer user_data);

/* Abort a currently running download */
gboolean glibwww_abort_request(GWWWRequest *request);

/* Get the progress of the currently running request.  nread or total may
 * return a negative result if it can't determine how far along things are. */
void glibwww_request_progress (GWWWRequest *request,
                               glong *nread, glong *total);


/* This is called by glibwww_init, but may be useful if you only want to
 * use the callbacks provided by glibwww for embedding libwww into the
 * glib main loop */
void glibwww_register_callbacks (void);

#endif

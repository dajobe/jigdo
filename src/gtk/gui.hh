#/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  GTK event handlers.
  Global variables: Pointers to GUI elements.

*/

#ifndef GTK_GUI_HH
#define GTK_GUI_HH

#include <config.h>
#include <interface.hh>
//______________________________________________________________________

/* Under Unix, packageDataDir is a string constant like
   "/usr/local/share/jigdo/" and is determined at compile time. Under
   Windows, it is a string containing the name of the dir with the
   .exe file. Similar for packageLocaleDir. */
#if WINDOWS
# include <string>
  extern string packageDataDir; // defined in jigdo.cc
# define packageLocaleDir (packageDataDir.c_str())
#else
# define packageDataDir PACKAGE_DATA_DIR
# define packageLocaleDir PACKAGE_LOCALE_DIR
#endif
//______________________________________________________________________

namespace GUI {

  // Are initialized by create()
  extern Window window;
  extern Filesel filesel;
  extern License license;
  // To be called by main() to set up the variables above
  void create();

} // namespace GUI
//______________________________________________________________________

// Callback prototypes for gtk-interface.cc

// Defined in gtk-gui.cc
void on_toolbarExit_clicked(GtkButton*, gpointer);
gboolean on_window_delete_event(GtkWidget*, GdkEvent*, gpointer);
void setNotebookPage(GtkWidget* pageObject);
void on_openButton_clicked(GtkButton*, gpointer);
void on_toolbarExit_clicked(GtkButton*, gpointer);
void on_aboutJigdoButton_clicked(GtkButton*, gpointer);

void on_download_startButton_enter(GtkButton*, gpointer);
void on_download_startButton_clicked(GtkButton*, gpointer);
void on_download_pauseButton_enter(GtkButton*, gpointer);
void on_download_pauseButton_clicked(GtkButton*, gpointer);
void on_download_stopButton_enter(GtkButton*, gpointer);
void on_download_stopButton_clicked(GtkButton*, gpointer);
void on_download_restartButton_enter(GtkButton*, gpointer);
void on_download_restartButton_clicked(GtkButton*, gpointer);
void on_download_closeButton_enter(GtkButton*, gpointer);
void on_download_closeButton_clicked(GtkButton*, gpointer);
void on_download_button_leave(GtkButton*, gpointer);

void on_jigdo_startButton_enter(GtkButton*, gpointer);
void on_jigdo_startButton_clicked(GtkButton*, gpointer);
void on_jigdo_pauseButton_enter(GtkButton*, gpointer);
void on_jigdo_pauseButton_clicked(GtkButton*, gpointer);
void on_jigdo_stopButton_enter(GtkButton*, gpointer);
void on_jigdo_stopButton_clicked(GtkButton*, gpointer);
void on_jigdo_restartButton_enter(GtkButton*, gpointer);
void on_jigdo_restartButton_clicked(GtkButton*, gpointer);
void on_jigdo_closeButton_enter(GtkButton*, gpointer);
void on_jigdo_closeButton_clicked(GtkButton*, gpointer);
void on_jigdo_button_leave(GtkButton*, gpointer);
//______________________________________________________________________

#endif

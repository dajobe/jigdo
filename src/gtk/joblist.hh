/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Interface to the GtkTreeView of running jobs (downloads etc),
  GUI::window.jobs, i.e. the list at the bottom of the jigdo window.

*/

#ifndef JOBLIST_HH
#define JOBLIST_HH

#include <config.h>

#include <string>
#include <vector>
#if DEBUG
#  include <iostream>
#endif

#include <debug.hh>
#include <download.hh>
#include <gui.hh>
#include <jobline.fh>
#include <log.hh>
#include <nocopy.hh>
//______________________________________________________________________

/** A bit like a vector<JobLine*>, but uses the GtkTreeStore for storing the
    elements. There can be empty entries in the vector which hold null
    pointers and which are displayed as empty lines on screen.

    ~JobList deletes all JobLine objects in the list. */
class JobList : NoCopy {
public:
  // Assumed number of columns in job display (progress bar, URL)
  enum {
    COLUMN_STATUS, // Pixmap "% done" + text message ("x kB/sec" etc)
    COLUMN_OBJECT, // URL
    COLUMN_DATA, // pointer to JobLine object (not displayed on screen)
    NR_OF_COLUMNS
  };
  static const int WIDTH_STATUS = 280;

  /* Time values are in milliseconds. All values should be multiples
     of TICK_INTERVAL */
  static const int TICK_INTERVAL = 250; // = progress report update interval

  LOCAL_DEBUG_UNIT_DECL;

  typedef unsigned size_type;
  inline JobList();
  /** Any Jobs still in the list are deleted */
  ~JobList();

  /** The GTK data structure that contains the linked list of items for this
      JobList. */
  inline GtkTreeStore* store() const;
  /** The GTK data structure responsible for drawing this list on screen.
      Currently, we cheat and always return the same static object,
      GUI::window.jobs, rather than storing a pointer in the JobList. This
      only works as long as only one JobList is ever created. */
  inline GtkTreeView* view() const;

  /** Size *includes* null pointer entries. */
  inline size_type size() const;
  /** Number of non-null entries (always <= size()) */
  inline size_type entryCount() const;
  inline bool empty() const;

  /** Retrieve the data for a list entry */
  inline JobLine* get(GtkTreeIter* row) const;
  /** Simply overwrites pointer, will not delete the old entry */
  inline void set(size_type n, JobLine* j);
  /** Deletes row from the list. The Job object pointed to by the entry is
   *not* deleted. */
  void erase(GtkTreeIter* row);
  /** Insert new JobLine before position n. Calls j->run() so the Job can
      visualize itself. */
  inline void insert(size_type n, JobLine* j);
  /** Add new JobLine at start of list, or as first child of parent if parent
      != 0. Also scrolls to the top to display the new entry. You should call
      j->run() after this so the Job can visualize itself. */
  void prepend(JobLine* j, JobLine* parent = 0);
  /** Append new JobLine at end of list or as last child of parent if parent
      != 0. You should call j->run() after this so the Job can visualize
      itself. */
  void append(JobLine* j, JobLine* parent = 0);
  /** Make row blank by setting text labels to "". Sets data pointer to 0,
      like erase() does *not* delete JobLine object of that row. */
  void makeRowBlank(GtkTreeIter* row);

  /** Set a static var of the JobList class to the supplied value. The Job
      that is currently selected and is in charge of updating the main window
      (e.g. with progress info) calls this with j==this, and subsequently
      uses isWindowOwner(this) to check whether it is still in charge of
      updating the window. This way, it is ensured that only one JobLine at a
      time updates the window. Supply 0 to unset. */
  inline void setWindowOwner(JobLine* j);
  /** Check whether the supplied JobLine is in charge of updating the
      window. */
  inline bool isWindowOwner(JobLine* j) const;
  /** Get the current JobLine in charge of the main window. This will be
      called if a button is clicked in the main window, to find the JobLine
      the click should be "forwarded" to. */
  inline JobLine* windowOwner() const;

  /** (De)register a JobLine whose tick() method should be called regularly.
      As soon as there is at least one such JobLine, a GTK timeout function
      is registered which does a freeze(), then scans through the whole list
      calling objects' tick handler where present, then thaw()s the list. As
      soon as the count of tick-needing JobLines reaches 0, the timeout
      function is unregistered again. */
  inline void registerTicks();
  inline void unregisterTicks();

  /** To be called sometime after gtk_init, but before any
      JobLines/JobVectors are used. */
  void postGtkInit();
# if DEBUG
  /** Perform internal integrity check, failed assertion if it fails. */
  void assertValid() const;
# else
  void assertValid() const { }
# endif

private:
  // GTK+ timeout function: calls tick() on stored JobLine objects
  static gint timeoutCallback(gpointer jobList);
  // GTK+ callback function, called when a line in the list is selected
  static gboolean selectRowCallback(GtkTreeSelection*, GtkTreeModel*,
                                    GtkTreePath*, gboolean, gpointer);
  /* Function registered with GTK+, sets up pixbuf with progress bar in the
     GtkTreeView. */
  static void pixbufForJobLine(GtkTreeViewColumn*, GtkCellRenderer* cell,
                               GtkTreeModel*, GtkTreeIter* iter,
                               gpointer data);
  // Load file with progress bar images, prepare it for display
  static void pixbufForJobLine_init();
  // Helper for prepend()
  static gboolean progressScrollToTop(gpointer view);
  // Set selectRowIdleId to 0
  static gboolean selectRowIdle(gpointer data);

  /* Used by initAfterGtk(): Nr of pixbufs to subdivide the progress XPM
     into, filename to load from. */
  static const int PROGRESS_SUBDIV = 61;
  static const char* const PROGRESS_IMAGE_FILE;
  static GdkPixbuf* progressImage; // Pixel data
  static vector<GdkPixbuf*> progressGfx; // sub-GdkPixbufs of progressImage
  static GValue progressValue;

  GtkTreeStore* storeVal; // GTK store of the displayed list

  unsigned sizeVal; // Number of rows in table
  unsigned entryCountVal; // Number of entries (= sizeVal - nr_of_empty_rows)
  JobLine* windowOwnerValue;

  /* Count the number of entries in the list which are in a state in which
     they need tick() calls. */
  int needTicks;
  int timeoutId; // as returned by gtk_timeout_add()

  // Callback avoids mult. calls to entries' selectRow()
  unsigned selectRowIdleId;
};
//______________________________________________________________________

/// Global list of running jobs
namespace GUI {
  extern JobList jobList;
}

//======================================================================

JobList::JobList() : storeVal(0), sizeVal(0), entryCountVal(0),
                     windowOwnerValue(0), needTicks(0),
                     selectRowIdleId(0) {
  // Mustn't access widgets here because GTK+ is not initialized yet!
}

JobList::size_type JobList::size() const { return sizeVal; }
bool JobList::empty() const { return sizeVal == 0; }
JobList::size_type JobList::entryCount() const { return entryCountVal; }
GtkTreeStore* JobList::store() const { return storeVal; }
GtkTreeView* JobList::view() const {
  return GTK_TREE_VIEW(GUI::window.jobs);
}

JobLine* JobList::get(GtkTreeIter* row) const {
  gpointer ptr;
  gtk_tree_model_get(GTK_TREE_MODEL(store()), row, COLUMN_DATA, &ptr, -1);
  return static_cast<JobLine*>(ptr);
}
#if 0
Job* JobList::get(size_type n) {
  return static_cast<JobLine*>(gtk_clist_get_row_data(list(), n));
}
void JobList::set(size_type n, JobLine* j) {
  gtk_clist_unselect_row(list(), n, 0);
  gtk_clist_set_row_data(list(), n, j);
  if (j) {
    j->jobVec = this;
    j->rowVal = n;
  }
}
void JobList::insert(size_type n, JobLine* j) {
  Paranoid(j != 0);
  gtk_clist_insert(list(), n, noText); // 0 => no text initially
  gtk_clist_set_row_data(list(), n, j);
  ++sizeVal;
  ++entryCountVal;
  j->jobVec = this;
  j->rowVal = n;
  j->run();
}
#endif

void JobList::registerTicks() {
  if (++needTicks == 1) {
    timeoutId = g_timeout_add(TICK_INTERVAL, timeoutCallback, this);
  }
  debug("registerTicks: %1", needTicks);
}
/* No further action is required. The timeout function will unregister itself
   next time it is called, by returning FALSE. */
void JobList::unregisterTicks() {
  if (--needTicks == 0)
    g_source_remove(timeoutId);
  debug("unregisterTicks: %1", needTicks);
}

void JobList::setWindowOwner(JobLine* j) { windowOwnerValue = j; }
bool JobList::isWindowOwner(JobLine* j) const { return windowOwnerValue==j; }
JobLine* JobList::windowOwner() const { return windowOwnerValue; }

#endif

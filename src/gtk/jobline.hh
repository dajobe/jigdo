/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  One line in a JobList, in the lower part of the jigdo GUI window

*/

#ifndef JOBLINE_HH
#define JOBLINE_HH

#include <config.h>
#include <gtk/gtk.h>

#include <nocopy.hh>
#include <joblist.hh>
//______________________________________________________________________

/** One "job", e.g. file download, scanning through files, or image download
    (which has many file downloads as children). Virtual class. */
class JobLine : NoCopy {
  friend class JobList; // Overwrites jobVec and rowVal on insert()
public:
  inline JobLine();

  /** If the JobLine was in a needTicks() state, jobList()->unregisterTicks()
      is called by this dtor. Additionally, removes the line from the
      GtkTreeList. */
  virtual ~JobLine();

  /** Start/restart this job. When called, must also create GUI elements for
      this Job in row number row() of GtkTreeView jobList()->view(). Is
      called after Job is added to the list, and when it is restarted (if at
      all). Child classes should provide an implementation.
      @return SUCCESS if everything OK, FAILURE if this object has deleted
      itself. */
  virtual bool run() = 0;

  /** Called when the JobLine's line is selected in the list. */
  virtual void selectRow() = 0;

  /** Is the job currently paused? */
  virtual bool paused() const = 0;
  /** Pause the job. */
  virtual void pause() = 0;
  /** Continue executing the job. */
  virtual void cont() = 0;
  /** Stop executing the job. */
  virtual void stop() = 0;

  /** When called, must fill in current nr of bytes downloaded and total nr
      of bytes. */
  virtual void percentDone(uint64* cur, uint64* total) = 0;

  /** Kind of a JobLine factory, called when the user clicks on OK to start a
      new download. It decides whether a normal file download or a .jigdo
      download is needed and creates the appropriate object. Additionally,
      the Job is appended to the list of jobs, and run. @param url What to
      download (if it ends in ".jigdo" and dest is a directory, will start
      jigdo processing) @param dest Where to put downloaded data, either a
      dir or a file. */
  static void create(const char* uri, const char* dest);

protected:
  typedef void (JobLine::*tickHandler)();
  /// Pointer to JobList
  inline JobList* jobList() const;
  /** Iterator for our row in the list. Do not modify the returned value,
      copy it instead! (It is non-const because various gtk functions which
      read it are incorrectly declared non-const.) NB: The returned iter is
      UNINITIALIZED until the JobLine is added to a JobList! */
  inline GtkTreeIter* row();

  /** Register a tick handler. The handler will be called every
      JobList::TICK_INTERVAL milliseconds by a GTK+ callback function that
      JobList registers. Handy for updating progress reports at regular
      intervals.
      Only register a handler if you really need it. Otherwise, pass 0 as the
      handler function pointer - if no JobLine at all registers a tick
      handler, the callback fnc will not be registered at all, saving some
      CPU time. */
  inline void callRegularly(tickHandler handler);
  /// Return current tick handler
  tickHandler getHandler() const { return tick; }
  /// Does this object need to be called regularly?
  bool needTicks() const { return tick != 0; }
  /** Wait appropriate nr of ticks, then register the supplied handler.
      Effectively, this means the JobLine pauses for a while - e.g. so the
      user can read some status report. */
  inline void callRegularlyLater(const int milliSec, tickHandler handler);

private:
  void waitTick();
  int waitCountdown;
  tickHandler waitDestination;

  JobList* jobVec;
  tickHandler tick;
  /* A reference to the row that this JobLine object is stored in.
     GtkTreeModel doc sez: GtkTreeStore and GtkListStore "guarantee that an
     iterator is valid for as long as the node it refers to is valid" */
  GtkTreeIter rowVal;
};

//======================================================================

JobLine::JobLine() : jobVec(0), tick(0) { }
JobList* JobLine::jobList() const { return jobVec; }
GtkTreeIter* JobLine::row() { return &rowVal; }
//________________________________________

void JobLine::callRegularly(tickHandler handler) {
  if (!needTicks() && handler != 0)
    jobList()->registerTicks(); // no previous handler, now handler
  else if (needTicks() && handler == 0)
    jobList()->unregisterTicks(); // previous handler present, now none
  tick = handler;
}

void JobLine::callRegularlyLater(const int milliSec, tickHandler handler) {
  waitCountdown = milliSec / JobList::TICK_INTERVAL;
  waitDestination = handler;
  callRegularly(&JobLine::waitTick);
}

#endif

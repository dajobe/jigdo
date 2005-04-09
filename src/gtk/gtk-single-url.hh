/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  'Simple' file download, i.e. download data and write it to a file.

*/

#ifndef GTK_SINGLE_URL_HH
#define GTK_SINGLE_URL_HH

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <bstream-counted.hh>
#include <jobline.hh>
#include <messagebox.hh>
#include <single-url.hh>
//______________________________________________________________________

/** This class is two things at once:

    1) Normal GtkSingleUrl mode: The frontend for a single-file download. It
    is a JobLine, so can be paused etc. like all JobLines. It registers as a
    Job::SingleUrl::IO with the Job::SingleUrl _that_it_owns_, to be notified
    whenever the job has something to say.

    2) GtkDataSource-which-just-isnt-called-like-that, aka "child mode": The
    frontend for a Job::DataSource object. This DataSource object is _not_
    owned by the GtkSingleUrl object. This is used if a MakeImageDl starts
    new child downloads.

    The two modes share so much code that IMHO doing two classes would not be
    better. */
class GtkSingleUrl : public JobLine, public Job::DataSource::IO {
public:
  /** Only in child mode, delay (millisec) between the MakeImageDl telling us
      that it has deleted its child and the moment we delete the
      corresponding line from the JobList. The delay allows the user to read
      the "finished" message. */
  static const int CHILD_FINISHED_DELAY = 10000;

  /** Create a new GtkSingleUrl, and also create an internal Job::SingleUrl
      to do the actual download. Delete the internal SingleURl from
      ~GtkSingleUrl(). */
  GtkSingleUrl(const string& uriStr, const string& destFile);
  /** Create a new GtkSingleUrl, but use it with an already existing
      Job::DataSource. The Job::DataSource will not be owned by the
      GtkSingleUrl and will not be deleted by its dtor. This mode is used to
      create "child downloads" during .jigdo processing: The IO object
      registered with the supplied Job::DataSource is set up to do with the
      data what it wants, and then to pass the call on to this GtkSingleUrl
      (more accurately, the methods that this GtkSingleUrl inherits from
      Job::DataSource::IO). See also GtkMakeImage::makeImageDl_new().

      @param uriStr URL
      @param destDesc A descriptive string like "/foo/bar/image, offset
      3453", NOT a filename! Supplied for information only, to be displayed
      to the user.
      @param download The download we are attached to */
  GtkSingleUrl(const string& uriStr, const string& destDesc,
               Job::DataSource* download);
  virtual ~GtkSingleUrl();

  // Virtual methods from JobLine
  virtual bool run();
  virtual void selectRow();
  virtual bool paused() const;
  virtual void pause();
  virtual void cont();
  virtual void stop();
  virtual void percentDone(uint64* cur, uint64* total);

  typedef void (GtkSingleUrl::*TickHandler)();
  inline void callRegularly(TickHandler handler);
  inline void callRegularlyLater(const int milliSec, TickHandler handler);

  // Called from gui.cc
  void on_startButton_clicked();
  inline void on_pauseButton_clicked();
  inline void on_stopButton_clicked();
  void on_restartButton_clicked();
  void on_closeButton_clicked();

  /** Cause this child download to remove itself from the JobList after
      CHILD_FINISHED_DELAY */
  inline void childIsFinished();

private:
  enum State {
    CREATED, RUNNING, PAUSED, STOPPED, ERROR, SUCCEEDED
  };

  // From Job::DataSource:IO
  virtual void job_deleted();
  virtual void job_succeeded();
  virtual void job_failed(const string& message);
  virtual void job_message(const string& message);
  virtual void dataSource_dataSize(uint64 n);
  virtual void dataSource_data(const byte* data, unsigned size,
                              uint64 currentSize);

  // Helper methods
  /* Registered to be called for each tick by run(), updates the "% done"
     progress info. */
  void showProgress();
  // Update info in main window
  void updateWindow();
  void resumeAsk(struct stat* fileInfo); // Ask user "resume/overwrite?"
  static void resumeResponse(GtkDialog*, int r, gpointer data);
  void openOutputAndRun(/*bool pragmaNoCache = false*/); // Allocate download job
  void openOutputAndResume(); // Alloc job and read resume data from file
  void updateTreeView(); // Update our line in GtkTreeView
  void failedPermanently(string* message);
  void startResume();
  // Reload with "Pragma: no-cache" header, discards previously fetched data
  void restart();
  /** Just executes "delete this". Used to remove the job line with
      callRegularlyLater() if this is a child download. */
  void deleteThis();
  // Only ask user if discarded data not worth more seconds than this
  static const int RESTART_WARNING_THRESHOLD = 30;
  static void afterStartButtonClickedResponse(GtkDialog*, int r, gpointer);
  static void afterCloseButtonClickedResponse(GtkDialog*, int r, gpointer);
  static void afterRestartButtonClickedResponse(GtkDialog*, int r, gpointer);

  /* true if the object was created using the second ctor, i.e. with a
     pre-created Job::DataSource object. */
  bool childMode;

  string uri; // Source URI
  string dest; // Destination filename
  string progress, status; // Lines to display in main window
  string treeViewStatus; // Status section in the list of jobs
  SmartPtr<BfstreamCounted> destStream;

  MessageBox::Ref messageBox;
  // If !childMode, the next two point to the same object!
  Job::DataSource* job;      // Pointer to SingleUrl or to DataSource object
  Job::SingleUrl* singleUrl; // Pointer to SingleUrl object, or null
  GTimeVal pauseStart; // timestamp of last download pause, or uninitialized

  State state;
};
//______________________________________________________________________

/* The static_cast from GtkSingleUrl::* to JobLine::* (i.e. member fnc of
   base class) is OK because we know for certain that the handler will only
   be invoked on SingleUrl objects. */
void GtkSingleUrl::callRegularly(TickHandler handler) {
  JobLine::callRegularly(static_cast<JobLine::TickHandler>(handler));
}
void GtkSingleUrl::callRegularlyLater(const int milliSec,
                                      TickHandler handler) {
  JobLine::callRegularlyLater(milliSec,
                              static_cast<JobLine::TickHandler>(handler));
}
//________________________________________

void GtkSingleUrl::on_pauseButton_clicked() {
  if (paused()) return;
  g_get_current_time(&pauseStart);
  pause();
  gtk_label_set_text(GTK_LABEL(GUI::window.download_buttonInfo), "");
}
void GtkSingleUrl::on_stopButton_clicked() {
  stop();
  gtk_label_set_text(GTK_LABEL(GUI::window.download_buttonInfo), "");
}
//________________________________________

void GtkSingleUrl::childIsFinished() {
  if (state == SUCCEEDED)
    callRegularlyLater(CHILD_FINISHED_DELAY, &GtkSingleUrl::deleteThis);
}

#endif

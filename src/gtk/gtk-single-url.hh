/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  'Simple' file download, i.e. download data and write it to a file.

*/

#ifndef GTK_SINGLE_URL_HH
#define GTK_SINGLE_URL_HH

#include <config.h>

#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <bstream.hh>
#include <jobline.hh>
#include <messagebox.hh>
#include <single-url.hh>
//______________________________________________________________________

/** The front-end for a single-file download. It is a JobLine, so can be
    paused etc. like all JobLines. It registers as a Job::SingleUrl::IO with
    the Job::SingleUrl that it owns, to be notified whenever the job has
    something to say.

    Additionally, it takes care of actually writing the downloaded data to
    the output file. This is a slight violation of our design "functionality
    only in the job classes", but writing to a file is such a simple
    operation... */
class GtkSingleUrl : public JobLine, public Job::SingleUrl::IO {
public:
  /** Create a new GtkSingleUrl, and also create an internal Job::SingleUrl
      to do the actual download. Delete the internal SingleURl from
      ~GtkSingleUrl(). */
  GtkSingleUrl(const string& uriStr, const string& destination);
  /** Create a new GtkSingleUrl, but use it with an already existing
      Job::SingleUrl. The Job::SingleUrl will not be owned by the
      GtkSingleUrl and will not be deleted by its dtor. If created like this,
      the downloaded data is never written to a file, it is simply discarded.
      This mode is used to create "child downloads" during .jigdo processing:
      The IO object registered with the supplied Job::SingleUrl is set up to
      do with the data what it wants, and then to pass the call on to this
      GtkSingleUrl (more accurately, the methods that this GtkSingleUrl
      inherits from Job::SingleUrl::IO). See also
      GtkMakeImage::makeImageDl_new(). */
  GtkSingleUrl(const string& uriStr, Job::SingleUrl* download);
  virtual ~GtkSingleUrl();

  // Virtual methods from JobLine
  virtual bool run();
  virtual void selectRow();
  virtual bool paused() const;
  virtual void pause();
  virtual void cont();
  virtual void stop();
  virtual void percentDone(uint64* cur, uint64* total);

  typedef void (GtkSingleUrl::*tickHandler)();
  inline void callRegularly(tickHandler handler);
  inline void callRegularlyLater(const int milliSec, tickHandler handler);

  // Called from gui.cc
  void on_startButton_clicked();
  inline void on_pauseButton_clicked();
  inline void on_stopButton_clicked();
  void on_restartButton_clicked();
  void on_closeButton_clicked();

private:
  // From Job::SingleUrl:IO
  virtual void job_deleted();
  virtual void job_succeeded();
  virtual void job_failed(string* message);
  virtual void job_message(string* message);
  virtual void singleURL_dataSize(uint64 n);
  virtual void singleURL_data(const byte* data, unsigned size,
                              uint64 currentSize);

  /* Return true if the object was created using the second ctor, i.e. with a
     pre-created Job::SingleUrl object. If true, will never write to any
     output file. */
  bool childMode() const { return dest.empty(); }

  // Helper methods
  /* Registered to be called for each tick by run(), updates the "% done"
     progress info. */
  void showProgress();
  // Update info in main window
  void updateWindow();
  void resumeAsk(struct stat* fileInfo); // Ask user "resume/overwrite?"
  static void resumeResponse(GtkDialog*, int r, gpointer data);
  void openOutputAndRun(bool pragmaNoCache = false); // Allocate download job
  void openOutputAndResume(); // Alloc job and read resume data from file
  void updateTreeView(); // Update our line in GtkTreeView
  void failedPermanently(string* message);
  void startResume();
  // Reload with "Pragma: no-cache" header, discards previously fetched data
  void restart();
  // Only ask user if discarded data not worth more seconds than this
  static const int RESTART_WARNING_THRESHOLD = 30;
  static void afterStartButtonClickedResponse(GtkDialog*, int r, gpointer);
  static void afterCloseButtonClickedResponse(GtkDialog*, int r, gpointer);
  static void afterRestartButtonClickedResponse(GtkDialog*, int r, gpointer);

  string uri; // Source URI
  string dest; // Destination filename
  string progress, status; // Lines to display in main window
  string treeViewStatus; // Status section in the list of jobs
  bfstream* destStream;

  MessageBox::Ref messageBox;
  Job::SingleUrl* job; // Job which handles the download
  GTimeVal pauseStart; // timestamp of last download pause, or uninitialized
};
//______________________________________________________________________

/* The static_cast from GtkSingleUrl::* to JobLine::* (i.e. member fnc of
   base class) is OK because we know for certain that the handler will only
   be invoked on SingleUrl objects. */
void GtkSingleUrl::callRegularly(tickHandler handler) {
  JobLine::callRegularly(static_cast<JobLine::tickHandler>(handler));
}
void GtkSingleUrl::callRegularlyLater(const int milliSec,
                                      tickHandler handler) {
  JobLine::callRegularlyLater(milliSec,
                              static_cast<JobLine::tickHandler>(handler));
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

#endif

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download and processing of .jigdo files - GTK+ frontend

*/

#ifndef GTK_MAKEIMAGE_HH
#define GTK_MAKEIMAGE_HH

#include <config.h>

#include <jobline.hh>
#include <makeimagedl.hh>
//______________________________________________________________________

class GtkMakeImage : public JobLine, private Job::MakeImageDl::IO {
public:
  GtkMakeImage(const string& uriStr, const string& destination);
  virtual ~GtkMakeImage();

  // Virtual methods from JobLine
  virtual bool run();
  virtual void selectRow();
  virtual bool paused() const;
  virtual void pause();
  virtual void cont();
  virtual void stop();
  virtual void percentDone(uint64* cur, uint64* total);

  typedef void (GtkMakeImage::*TickHandler)();
  inline void callRegularly(TickHandler handler);
  inline void callRegularlyLater(const int milliSec, TickHandler handler);

  // Called from gui.cc
  void on_startButton_clicked();
  void on_pauseButton_clicked();
  void on_stopButton_clicked();
  void on_restartButton_clicked();
  void on_closeButton_clicked();

private:
  // Virtual methods from Job::MakeImageDl::IO:
  virtual void job_deleted();
  virtual void job_succeeded();
  virtual void job_failed(string* message);
  virtual void job_message(string* message);
  virtual Job::DataSource::IO* makeImageDl_new(
      Job::DataSource* childDownload, const string& uri,
      const string& destDesc);
  virtual void makeImageDl_finished(Job::DataSource* childDownload,
                                    Job::DataSource::IO* yourIo);

  // Update info in main window
  void updateWindow();

  string progress, status; // Lines to display in main window
  string treeViewStatus; // Status section in the list of jobs

  Job::MakeImageDl mid;
};
//______________________________________________________________________

/* The static_cast from GtkMakeImage::* to JobLine::* (i.e. member fnc of
   base class) is OK because we know for certain that the handler will only
   be invoked on SingleUrl objects. */
void GtkMakeImage::callRegularly(TickHandler handler) {
  JobLine::callRegularly(static_cast<JobLine::TickHandler>(handler));
}
void GtkMakeImage::callRegularlyLater(const int milliSec,
                                      TickHandler handler) {
  JobLine::callRegularlyLater(milliSec,
                              static_cast<JobLine::TickHandler>(handler));
}

#endif

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

#include <config.h>

#include <time.h>

#include <autoptr.hh>
#include <gtk-single-url.hh>
#include <joblist.hh>
#include <messagebox.hh>
#include <string-utf.hh>
//______________________________________________________________________

GtkSingleUrl::GtkSingleUrl(const string& uriStr, const string& destination)
  : uri(uriStr), dest(destination), progress(), status(), treeViewStatus(),
    destStream(0), messageBox(), job(0) {
  // Must supply an output file. destination.empty() is used by childMode()
  Assert(!destination.empty());
}

GtkSingleUrl::GtkSingleUrl(const string& uriStr, Job::SingleUrl* download)
  : uri(uriStr), dest(), progress(), status(), treeViewStatus(),
    destStream(0), messageBox(), job(download) { }

GtkSingleUrl::~GtkSingleUrl() {
  if (jobList()->isWindowOwner(this))
    setNotebookPage(GUI::window.pageOpen);
  if (childMode()) {
    if (job != 0) job->setDestStream(0);
  } else {
    // Only delete if job owned, i.e. if we're not a child of another job
    delete job;
    Paranoid(job == 0); // Was set to 0 during above stmt by job_deleted()
  }
  if (job != 0) {
    // Ensure we're no longer called if anything happens to the download
    IOPtr<Job::SingleUrl::IO>& io(job->io());
    if (io.get() == this) {
      io.release();
    }
  }
  delete destStream;
}
//______________________________________________________________________

// Regular file download (not .jigdo download)
bool GtkSingleUrl::run() {

  // Show URL as object name
  progress.erase();
  status = _("Waiting...");
  treeViewStatus = _("Waiting");
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     JobList::COLUMN_OBJECT, uri.c_str(),
                     -1);

  // Don't open output file and don't run() download if child mode
  if (dest.empty()) {
    Paranoid(childMode());
    return SUCCESS;
  }

  struct stat fileInfo;
  int statResult = stat(dest.c_str(), &fileInfo);
  if (statResult == 0) {
    if (S_ISDIR(fileInfo.st_mode)) {
      MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK,
        _("Error accessing destination"),
        subst(_("Cannot save to `%LE1' because that already exists as a "
                "directory"), dest));
      m->show();
      delete this;
      return FAILURE;
    } else if (S_ISREG(fileInfo.st_mode)) {
      // File exists - ask user
      resumeAsk(&fileInfo);
      return SUCCESS;
    } else {
      // Destination is device or link
      MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK,
        _("Error accessing destination"),
        subst(_("Cannot save to `%LE1' because that already exists as a "
                "device/link"), dest));
      m->show();
      messageBox.set(m); //delete this;
      return FAILURE;
    }
  }
  openOutputAndRun();
  return SUCCESS;
}
//______________________________________________________________________

void GtkSingleUrl::openOutputAndRun(bool pragmaNoCache) {
  // Open output file
  Paranoid(!childMode());
  Paranoid(destStream == 0);
  destStream = new bfstream(dest.c_str(),
                            ios::binary|ios::in|ios::out|ios::trunc);
  if (!*destStream) {
    MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK,
      _("Error accessing destination"),
      subst(_("It was not possible to open `%LE1' for output: %LE2"),
            dest, strerror(errno)));
    m->show();
    messageBox.set(m);
    return;
  }

  // Allocate job and run it
  status = _("Waiting...");
  if (job == 0)
    job = new Job::SingleUrl(this, uri);
  job->run(0, destStream, 0, 0, pragmaNoCache);
}
//________________________________________

#if 0
namespace {

  /** Create a byte array of the indicated size and load into it bytes from
      the stream, starting at offset startOff. */
  byte* loadData(bfstream* s, uint64 startOff, uint64 size) {
    s->seekp(startOff);
    if (!*s) return 0;

    byte* buf = new byte[size];
    ArrayAutoPtr<byte> bufDel(buf);

    // Read data from file
    byte* bufPos = buf;
    while (*s && size > 0) {
      readBytes(*s, bufPos, size);
      size_t n = s->gcount();
      bufPos += n;
      size -= n;
    }
    if (!*s) return 0; // bufDel will delete buf
    return bufDel.release();
  }

}
#endif
//________________________________________

void GtkSingleUrl::openOutputAndResume() {
  Paranoid(!childMode());
  struct stat fileInfo;
  int statResult = stat(dest.c_str(), &fileInfo);

  if (statResult == 0) {
    if (destStream != 0) delete destStream;
    destStream = new bfstream(dest.c_str(), ios::binary|ios::in|ios::out);
    if (*destStream) {
      // Start the resume download
      status = subst(_("Resuming download - overlap is %1kB"),
                     Job::SingleUrl::RESUME_SIZE / 1024);
      updateWindow();
      if (job == 0)
        job = new Job::SingleUrl(this, uri);
      job->run(fileInfo.st_size, destStream, 0, 0, false);
      return;
    }
  }

  // An error occurred
# if DEBUG
  cerr << "GtkSingleUrl::openOutputAndResume: statResult=" << statResult
       << ' ' << strerror(errno) << endl;
# endif
  treeViewStatus = _("<b>Open of output file failed</b>");
  string error = subst(_("Could not open output file: %L1"),
                       strerror(errno));
  failedPermanently(&error);
}
//______________________________________________________________________

/* Called e.g. after the connection dropped unexpectedly, to resume the
   download. */
void GtkSingleUrl::startResume() {
  Paranoid(!childMode());
  Paranoid(job != 0);
  Paranoid(job->resumePossible()); // We already checked this earlier
  callRegularly(0);

  Assert(destStream == 0);
  destStream = new bfstream(dest.c_str(), ios::binary|ios::in|ios::out);
  if (!*destStream) {
    // An error occurred
    treeViewStatus = _("<b>Open of output file failed</b>");
    string error = subst(_("Could not open output file: %L1"),
                         strerror(errno));
    failedPermanently(&error);
    return;
  }

  // Start the resume download
  status = subst(_("Resuming download - overlap is %1kB"),
                 Job::SingleUrl::RESUME_SIZE / 1024);
  updateWindow();
  job->run(0, destStream, 0, 0, false);
  return;
}
//______________________________________________________________________

void GtkSingleUrl::selectRow() {
  setNotebookPage(GUI::window.pageDownload);
  jobList()->setWindowOwner(this);
  updateWindow();
}
//______________________________________________________________________

bool GtkSingleUrl::paused() const {
  return job != 0 && job->paused();
}

void GtkSingleUrl::pause() {
  if (paused()) return;
  if (job != 0) job->pause();
  //progress.clear();
  status = _("Download is paused");
  updateWindow();
  showProgress(); // Display "50kB of 100kB, paused" in GtkTreeView
}

void GtkSingleUrl::cont() {
  if (!paused()) return;
  if (job != 0) job->cont();
  status.erase();
  updateWindow();
  progress.erase();
}

void GtkSingleUrl::stop() {
# if DEBUG
  //#warning "TODO what if childMode"
  cerr << "Stopping SingleUrl " << (job != 0 ? job : 0) << " at byte "
       << job->progress()->currentSize() << endl;
# endif
  destStream->sync();
  delete destStream;
  destStream = 0;
  if (job != 0) {
    job->setDestStream(0);
    if (job->paused()) job->cont();
    job->stop();
  }
  progress.erase();
  status = _("Download was stopped manually");
  updateWindow();
}
//______________________________________________________________________

void GtkSingleUrl::percentDone(uint64* cur, uint64* total) {
  if (job == 0 || job->progress()->dataSize() == 0) {
    *cur = *total = 0;
  } else {
    *cur = job->progress()->currentSize();
    *total = job->progress()->dataSize();
  }
}
//______________________________________________________________________

void GtkSingleUrl::updateWindow() {
  if (!jobList()->isWindowOwner(this)) return;

  // URL and destination lines
  gtk_label_set_text(GTK_LABEL(GUI::window.download_URL), uri.c_str());
  gtk_label_set_text(GTK_LABEL(GUI::window.download_dest), dest.c_str());

  // Progress and status lines
  if (job != 0 && !job->paused() && !job->failed()) {
    progress.erase();
    job->progress()->appendProgress(&progress);
  }
  gtk_label_set_text(GTK_LABEL(GUI::window.download_progress),
                     progress.c_str());
  gtk_label_set_text(GTK_LABEL(GUI::window.download_status),
                     status.c_str());

  // Buttons (in)sensitive
  gtk_widget_set_sensitive(GUI::window.download_startButton,
    (job != 0 && (paused() || job->succeeded()
                  || job->failed() && job->resumePossible()) ?
     TRUE : FALSE));
  gtk_widget_set_sensitive(GUI::window.download_pauseButton,
    (job != 0 && !job->failed() && !job->succeeded() && !paused() ?
     TRUE : FALSE));
  gtk_widget_set_sensitive(GUI::window.download_stopButton,
    (job != 0 && !job->failed() && !job->succeeded() ?
     TRUE : FALSE));
}
//______________________________________________________________________

// fileInfo is stat() result of destination filename
void GtkSingleUrl::resumeAsk(struct stat* fileInfo) {
  // How old is the file in minutes?
  time_t fileAge = (time(0) - fileInfo->st_mtime + 30) / 60;
  int days = fileAge / (60*24);
  int hours = fileAge / 60 - days * 24;
  int minutes = fileAge - (days * 60*24) - (hours * 60);
  string age;
  if (days > 0)
    age = subst(_("%1 days and %2 hours"), days, hours);
  else if (hours > 0)
    age = subst(_("%1 hours and %2 minutes"), hours, minutes);
  else
    age = subst(_("%1 minutes"), minutes);

  MessageBox* m = new MessageBox(MessageBox::QUESTION, MessageBox::NONE,
    _("Output file exists - overwrite it or resume download?"),
    subst(_("The output file `%LE1' already exists with a size of %2 "
            "bytes, it is %3 old.\n"
            "<b>Overwrite</b> deletes the data in this file, whereas "
            "<b>Resume</b> can be used to continue an earlier, interrupted "
            "download of the same file."),
          dest, static_cast<uint64>(fileInfo->st_size), age));
  m->addStockButton("gtk-cancel", GTK_RESPONSE_CANCEL);
  m->addButton(_("_Overwrite"), 0);
  m->addButton(_("_Resume"), 1);
  m->onResponse(&resumeResponse, this);
  m->show();
  messageBox.set(m);
  progress.erase();
  status = _("Please answer the pop-up: Overwrite, resume or cancel?");
  updateWindow();
  messageBox.set(m);
}

void GtkSingleUrl::resumeResponse(GtkDialog*, int r, gpointer data) {
  GtkSingleUrl* self = static_cast<GtkSingleUrl*>(data);

  if (r == GTK_RESPONSE_CANCEL) {
    delete self;
    return;
  }

  // Remove "please react to pop-up" message
  self->status.erase();

  if (r == 0) {
    // Overwrite
    self->openOutputAndRun();
  } else {
    // Resume
    Paranoid(r == 1);
    self->openOutputAndResume();
  }

  self->updateWindow();
  self->messageBox.set(0);
  /* Must call this now and not before the "if" above becase
     openOutputAndResume() makes paused()==false, which is what should be
     displayed */
  if (self->jobList()->isWindowOwner(self)) self->updateWindow();
}
//______________________________________________________________________

/* The job whose io ptr references us is being deleted. */
void GtkSingleUrl::job_deleted() {
  job = 0;
  return;
}
//______________________________________________________________________

void GtkSingleUrl::job_succeeded() {
  callRegularly(0);

  // Can't see the same problem as with job_failed(), but just to be safe...
  delete destStream;
  destStream = 0;
  job->setDestStream(0);
  status = _("Download is complete");
  updateWindow();
  string s;
  Progress::appendSize(&s, job->progress()->currentSize());
  s = subst(_("Finished - fetched %1"), s);
  job_message(&s);
}
//______________________________________________________________________

/* Like job_failed() below, but don't pay attention whether
   job->resumePossible() - *never* auto-resume the download. */
void GtkSingleUrl::failedPermanently(string* message) {
  job->setDestStream(0);
  delete destStream;
  destStream = 0;
  treeViewStatus = subst(_("<b>%1</b>"), message);
  status.swap(*message);
  updateWindow();
  gtk_tree_store_set(jobList()->store(), row(), JobList::COLUMN_STATUS,
                     treeViewStatus.c_str(), -1);
}
//______________________________________________________________________

void GtkSingleUrl::job_failed(string* message) {
  /* Important: close the file here. Otherwise, the following can happen: 1)
     Download aborts with an error, old JobLine still displays error message,
     isn't deleted yet; its stream stays open. 2) User restarts download, a
     SECOND stream is created. It's created with ios::trunc, but that's
     ignored (under Linux) cos of the second open handle. 3) If the second
     JobLine doesn't overwrite all data from the first one, we end up with
     file contents like "data from 2nd download" + "up to a page full of null
     bytes" + "stale data from 1st download". */
  job->setDestStream(0);
  delete destStream;
  destStream = 0;

  bool resumePossible = (job != 0 && job->resumePossible());
  if (resumePossible) {
    treeViewStatus = subst(_("Try %1 of %2 after <b>%E3</b>"),
                           job->currentTry() + 1, Job::SingleUrl::MAX_TRIES,
                           message);
  } else {
    treeViewStatus = subst(_("<b>%E1</b>"), message);
  }
  status.swap(*message);
  updateWindow();
  gtk_tree_store_set(jobList()->store(), row(), JobList::COLUMN_STATUS,
                     treeViewStatus.c_str(), -1);
  if (resumePossible)
    callRegularlyLater(Job::SingleUrl::RESUME_DELAY,
                       &GtkSingleUrl::startResume);
#if DEBUG
  //#warning "TODO if childMode, pause, then delete ourself"
#endif
}
//______________________________________________________________________

void GtkSingleUrl::job_message(string* message) {
  treeViewStatus.swap(*message);
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     -1);
}
//______________________________________________________________________

// Don't need this info - ignore
void GtkSingleUrl::singleUrl_dataSize(uint64) {
  return;
}
//______________________________________________________________________

void GtkSingleUrl::singleUrl_data(const byte* /*data*/, unsigned /*size*/,
                                  uint64 /*currentSize*/) {
# if DEBUG
  cerr<<"GtkSingleUrl::singleUrl_data "<<job->progress()->currentSize()<<endl;
# endif
  if (!needTicks())
    callRegularly(&GtkSingleUrl::showProgress);

  if (childMode()) return;

  //writeBytes(*destStream, data, size);
  if (!*destStream) {
    /* According to Stroustrup, "all bets are off" WRT the state of the
       stream. We assume that this does *not* mean that junk has been
       written, only that no all 'size' bytes were written. */
    treeViewStatus = _("<b>Write to output file failed</b>");
    string error = subst(_("Could not write to output file: %L1"),
                         strerror(errno));
    failedPermanently(&error);
  }
}
//______________________________________________________________________

// Show progress info.
void GtkSingleUrl::showProgress() {
//   bool isOwner = jobList()->isWindowOwner(this);
//   if (progress.currentSize() == 0) {
//     // No data received yet - set label blank
//     if (isOwner) {
//       gtk_label_set_text(GTK_LABEL(GUI::window.download_progress), "");
//       gtk_label_set_text(GTK_LABEL(GUI::window.download_status), "");
//     }
//     return;
//   }

//  cerr<<*job->progress()<<endl;

  if (job == 0 || job->failed() || job->succeeded()) return;

  GTimeVal now;
  g_get_current_time(&now);
  string s;

  const Progress* jobProgress = job->progress();
  int timeLeft = jobProgress->timeLeft(now);
  int speed = 0;

  treeViewStatus.erase();
  // Append "50kB" if size not known, else "50kB of 10MB"
  Progress::appendSizes(&treeViewStatus, jobProgress->currentSize(),
                        jobProgress->dataSize());
  // Append "10kB/s"
  if (paused()) {
    // Switch off calls to updateProgress() until user clicks Continue
    callRegularly(0);
    treeViewStatus += _(", paused");
  } else {
    speed = jobProgress->speed(now);
    if (speed == 0) {
      treeViewStatus += _(", stalled");
    } else if (speed != -1) {
      treeViewStatus += _(", ");
      Progress::appendSize(&treeViewStatus, speed);
      treeViewStatus += _("/s");
    }
  }
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     -1);
  //____________________

  if (jobList()->isWindowOwner(this)) {
    // Percentage/kBytes/bytes done in main window
    progress.erase();
    jobProgress->appendProgress(&progress);
    gtk_label_set_text(GTK_LABEL(GUI::window.download_progress),
                       progress.c_str());

    // Speed/ETA in main window
    if (!paused()) {
      status.erase();
      jobProgress->appendSpeed(&status, speed, timeLeft);
      gtk_label_set_text(GTK_LABEL(GUI::window.download_status),
                         status.c_str());
    }
  }
}
//______________________________________________________________________

void GtkSingleUrl::on_startButton_clicked() {
  if (paused()) {
    cont();
    gtk_label_set_text(GTK_LABEL(GUI::window.download_buttonInfo), "");
    return;
  }

  Assert(job != 0 && (job->failed() || job->succeeded()));
  if (job == 0 || (!job->failed() && !job->succeeded())) return;

  // Resume download

  struct stat fileInfo;
  int statResult = stat(dest.c_str(), &fileInfo);

  // Scream if file no longer there, or not a file
  if (statResult != 0 || !S_ISREG(fileInfo.st_mode)) {
    string error;
    if (statResult != 0)
      error = subst(_("Resuming `%LE1' is not possible: %LE2"),
                    dest, strerror(errno));
    else
      error = subst(_("Resuming `%LE1' is not possible: It is not a regular "
                      "file"), dest);
    MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK,
                                   _("Error accessing output file"), error);
    m->show();
    messageBox.set(m);
    return;
  }

  // Scream if file/file size changed since download was stopped
  const Progress* jobProgress = job->progress();
  uint64 size = static_cast<uint64>(fileInfo.st_size);
  if (jobProgress->currentSize() != size) {
    MessageBox* m = new MessageBox(MessageBox::WARNING, MessageBox::NONE,
      _("Size of output file changed"),
      subst(_("When the download was stopped, the length of the output file "
              "`%LE1' was %2 bytes, but now it is %3 bytes. Do you really "
              "want to resume the download (from byte %3)?"),
            dest, jobProgress->currentSize(), size));
    m->addButton(_("_Resume"), 0);
    m->addStockButton("gtk-cancel", GTK_RESPONSE_CANCEL);
    m->onResponse(&afterStartButtonClickedResponse, this);
    m->show();
    messageBox.set(m);
    return;
  }

  // All OK, resume
  openOutputAndResume();
  updateWindow();
  gtk_label_set_text(GTK_LABEL(GUI::window.download_buttonInfo), "");
}

void GtkSingleUrl::afterStartButtonClickedResponse(GtkDialog*, int r,
                                                   gpointer data) {
  GtkSingleUrl* self = static_cast<GtkSingleUrl*>(data);

  self->messageBox.set(0);
  if (r == GTK_RESPONSE_CANCEL) return;
  self->openOutputAndResume();
  self->updateWindow();
  gtk_label_set_text(GTK_LABEL(GUI::window.download_buttonInfo), "");
}
//______________________________________________________________________

void GtkSingleUrl::on_closeButton_clicked() {
  if (job == 0
      || paused() || job->failed() || job->succeeded()
      || job->progress()->currentSize() == 0) {
    delete this;
    return;
  }

  MessageBox* m = new MessageBox(MessageBox::WARNING, MessageBox::NONE,
    _("Download in progress - really abort it?"), 0);
  m->addStockButton("gtk-cancel", GTK_RESPONSE_CANCEL);
  m->addButton(_("_Abort download"), 0);
  m->onResponse(&afterCloseButtonClickedResponse, this);
  m->show();
  messageBox.set(m);
}

void GtkSingleUrl::afterCloseButtonClickedResponse(GtkDialog*, int r,
                                                   gpointer data) {
  if (r == GTK_RESPONSE_CANCEL) return;
  GtkSingleUrl* self = static_cast<GtkSingleUrl*>(data);
  if (DEBUG) cerr << "GtkSingleUrl::afterCloseButtonClickedResponse: "
                     "deleting " << self << endl;
  delete self;
}
//______________________________________________________________________

void GtkSingleUrl::on_restartButton_clicked() {
  messageBox.set(0); // Close "overwrite/resume/cancel" question, if any
  if (job == 0 || job->failed()) {
    restart();
    return;
  }

  /* Trying to be clever here (oh dear...): Only ask whether to discard
     existing data if downloading that data took longer than half a minute
     (RESTART_WARNING_THRESHOLD seconds). The idea is that a wrong click will
     at most cost you 30 secs to recover, which happens to be the limit
     recommended by the GNOME usability people. :) */
  const Progress* jobProgress = job->progress();
  unsigned speed;
  if (paused()) {
    speed = jobProgress->speed(pauseStart); // speed = bytes per sec
  } else {
    GTimeVal now;
    g_get_current_time(&now);
    speed = jobProgress->speed(now); // speed = bytes per sec
  }
  /* speed will be 0 after Pause+Stop, in that case just assume an arbitrary
     speed of 64kB/sec, which is better than nothing... */
  if (speed == 0) speed = 65536;
# if DEBUG
  cerr << "GtkSingleUrl::on_restartButton_clicked: cur="
       << jobProgress->currentSize() << " speed=" << speed << " thresh="
       << speed * RESTART_WARNING_THRESHOLD << endl;
# endif
  if (speed == 0
      || jobProgress->currentSize() < speed * RESTART_WARNING_THRESHOLD) {
    restart();
    return;
  }

  MessageBox* m = new MessageBox(MessageBox::WARNING, MessageBox::NONE,
    _("Restarting will discard already downloaded data"),
    _("Some data has already been downloaded. Are you sure you want to "
      "delete this data and restart the download?"));
  m->addStockButton("gtk-cancel", GTK_RESPONSE_CANCEL);
  m->addButton(_("_Restart"), 0);
  m->onResponse(&afterRestartButtonClickedResponse, this);
  m->show();
  messageBox.set(m);
}

void GtkSingleUrl::afterRestartButtonClickedResponse(GtkDialog*, int r,
                                                     gpointer data) {
  if (r == GTK_RESPONSE_CANCEL) return;
  GtkSingleUrl* self = static_cast<GtkSingleUrl*>(data);
  self->restart();
}

void GtkSingleUrl::restart() {
  // Kill the old download
  delete destStream;
  destStream = 0;
  if (job != 0) {
    job->setDestStream(0);
    Assert(!childMode());
    if (job->paused()) job->cont();
    job->stop();
//     delete job;
//     job = 0;
  }

  // Start the new download
  openOutputAndRun(true);
  progress.erase();
  status = _("Download was restarted - waiting...");
  treeViewStatus = _("Restarted - waiting");
  updateWindow();
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     -1);
}

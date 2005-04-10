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
#include <errno.h>

#include <autoptr.hh>
#include <gtk-single-url.hh>
#include <joblist.hh>
#include <log.hh>
#include <messagebox.hh>
#include <string-utf.hh>
//______________________________________________________________________

DEBUG_UNIT("gtk-single-url")

// Non-child mode, will create our own SingleUrl
GtkSingleUrl::GtkSingleUrl(const string& uriStr, const string& destFile)
  : childMode(false), uri(uriStr), dest(destFile), progress(), status(),
    treeViewStatus(), destStream(0), messageBox(), job(0), singleUrl(0),
    state(CREATED) {
  debug("GtkSingleUrl %1, not child", this);
}

// Child mode, using supplied DataSource
GtkSingleUrl::GtkSingleUrl(const string& uriStr, const string& destDesc,
                           Job::DataSource* download)
  : childMode(true), uri(uriStr), dest(destDesc), progress(), status(),
    treeViewStatus(), destStream(0), messageBox(), job(download),
    singleUrl(0), state(CREATED) {
  debug("GtkSingleUrl %1, is child", this);
}

GtkSingleUrl::~GtkSingleUrl() {
  debug("~GtkSingleUrl %1", childMode);
  callRegularly(0);
  if (jobList()->isWindowOwner(this))
    setNotebookPage(GUI::window.pageOpen);
  if (!childMode) {
    // Only delete if job owned, i.e. if we're not a child of another job
    debug("Deleting job %1", job);
    delete job;
    Paranoid(job == 0); // Was set to 0 during above stmt by job_deleted()
  }
  debug("~GtkSingleUrl done");
}
//______________________________________________________________________

// Regular file download (not .jigdo download)
bool GtkSingleUrl::run() {

  state = RUNNING;

  // Show URL as object name
  progress.erase();
  status = _("Waiting...");
  treeViewStatus = _("Waiting");
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     JobList::COLUMN_OBJECT, uri.c_str(),
                     -1);

  // Don't open output file and don't run() download if child mode
  if (childMode) return SUCCESS;

  struct stat fileInfo;
  int statResult = stat(dest.c_str(), &fileInfo);
  if (statResult == 0) {
    if (S_ISDIR(fileInfo.st_mode)) {
      MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK,
        _("Error accessing destination"),
        subst(_("Cannot save to `%LE1' because that already exists as a "
                "directory"), dest));
      m->show();
      state = ERROR;
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
      state = ERROR;
      messageBox.set(m); //delete this;
      return FAILURE;
    }
  }
  openOutputAndRun();
  return SUCCESS;
}
//______________________________________________________________________

void GtkSingleUrl::openOutputAndRun(/*bool pragmaNoCache*/) {
  // Open output file
  Paranoid(!childMode);
  Paranoid(destStream == 0);
  destStream = new BfstreamCounted(dest.c_str(),
                                   ios::binary|ios::in|ios::out|ios::trunc);
  if (!*destStream) {
    MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK,
      _("Error accessing destination"),
      subst(_("Could not open `%LE1' for output: %LE2"),
            dest, strerror(errno)));
    m->show();
    state = ERROR;
    messageBox.set(m);
    return;
  }

  // Allocate job and run it
  status = _("Waiting...");
  if (job == 0) job = singleUrl = new Job::SingleUrl(uri);
  debug("singleUrl=%1", singleUrl);
  singleUrl->io.addListener(*this);
  singleUrl->setDestination(destStream.get(), 0, 0);
  //singleUrl->setPragmaNoCache(pragmaNoCache);
  singleUrl->run();
}
//________________________________________

void GtkSingleUrl::openOutputAndResume() {
  Paranoid(!childMode);
  Paranoid(destStream == 0);
  struct stat fileInfo;
  int statResult = stat(dest.c_str(), &fileInfo);

  if (statResult == 0) {
    //if (destStream != 0) delete destStream;
    destStream = new BfstreamCounted(dest.c_str(),
                                     ios::binary|ios::in|ios::out);
    if (*destStream) {
      // Start the resume download
      state = RUNNING;
      status = subst(_("Resuming download - overlap is %1kB"),
                     Job::SingleUrl::RESUME_SIZE / 1024);
      updateWindow();
      if (job == 0) job = singleUrl = new Job::SingleUrl(uri);
      debug("singleUrl=%1", singleUrl);
      iList_remove();
      singleUrl->io.addListener(*this);
      singleUrl->setResumeOffset(fileInfo.st_size);
      singleUrl->setDestination(destStream.get(), 0, 0);
      singleUrl->run();
      return;
    }
  }

  // An error occurred
  debug("openOutputAndResume: statResult=%1, %2", statResult,
        strerror(errno));
  treeViewStatus = _("<b>Open of output file failed</b>");
  string error = subst(_("Could not open output file: %LE1"),
                       strerror(errno));
  failedPermanently(&error);
}
//______________________________________________________________________

/* Auto-resume, called e.g. after the connection dropped unexpectedly, to
   resume the download. */
void GtkSingleUrl::startResume() {
  Paranoid(!childMode);
  Paranoid(job != 0);
  Paranoid(singleUrl->resumePossible()); // We already checked this earlier
  callRegularly(0);

  Assert(destStream == 0);
# if DEBUG
  struct stat fileInfo;
  int statResult = stat(dest.c_str(), &fileInfo);
  Paranoid(statResult == 0);
  debug("startResume: Trying resume from %1, actual file size %2",
        job->progress()->currentSize(), uint64(fileInfo.st_size));
  Paranoid(job->progress()->currentSize() == uint64(fileInfo.st_size));
# endif
  destStream = new BfstreamCounted(dest.c_str(),
                                   ios::binary|ios::in|ios::out);
  if (!*destStream) {
    // An error occurred
    treeViewStatus = _("<b>Open of output file failed</b>");
    string error = subst(_("Could not open output file: %LE1"),
                         strerror(errno));
    failedPermanently(&error);
    return;
  }

  // Start the resume download
  state = RUNNING;
  status = subst(_("Resuming download - overlap is %1kB"),
                 Job::SingleUrl::RESUME_SIZE / 1024);
  updateWindow();
  iList_remove();
  singleUrl->io.addListener(*this);
  singleUrl->setResumeOffset(job->progress()->currentSize());
  singleUrl->setDestination(destStream.get(), 0, 0);
  singleUrl->run();
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
  return job != 0 && state == PAUSED;
}

void GtkSingleUrl::pause() {
  if (paused()) return;
  if (job != 0) {
    job->pause();
    state = PAUSED;
  }
  //progress.clear();
  status = _("Download is paused");
  updateWindow();
  showProgress(); // Display "50kB of 100kB, paused" in GtkTreeView
}

void GtkSingleUrl::cont() {
  if (!paused()) return;
  if (job != 0) {
    job->cont();
    state = RUNNING;
  }
  status.erase();
  updateWindow();
  progress.erase();
}

void GtkSingleUrl::stop() {
  Paranoid(!childMode);
  debug("Stopping SingleUrl %1 at byte %2",
        singleUrl, singleUrl->progress()->currentSize());
  destStream->sync();
  destStream.clear();
  if (job != 0) {
    singleUrl->setDestination(0, 0, 0);
    if (state == PAUSED) singleUrl->cont();
    state = STOPPED;
    singleUrl->stop();
  }
  //progress.erase();
  status = _("Download was stopped manually");
  updateWindow();
}
//______________________________________________________________________

void GtkSingleUrl::percentDone(uint64* cur, uint64* total) {
  if (job == 0 || job->progress()->dataSize() == 0) {
    if (state == SUCCEEDED) *cur = *total = 1;
    else *cur = *total = 0;
  } else {
    *cur = job->progress()->currentSize();
    *total = job->progress()->dataSize();
  }
}
//______________________________________________________________________

void GtkSingleUrl::updateWindow() {
  if (!jobList()->isWindowOwner(this)) return;

  debug("updateWindow: state=%1 status=\"%2\"", int(state), status);

  // URL and destination lines
  gtk_label_set_text(GTK_LABEL(GUI::window.download_URL), uri.c_str());
  gtk_label_set_text(GTK_LABEL(GUI::window.download_dest), dest.c_str());

  // Progress and status lines
  if (job != 0 && (state == RUNNING || state == STOPPED)) {
    progress.erase();
    job->progress()->appendProgress(&progress);
  }
  gtk_label_set_text(GTK_LABEL(GUI::window.download_progress),
                     progress.c_str());
  gtk_label_set_text(GTK_LABEL(GUI::window.download_status),
                     status.c_str());

  // Buttons (in)sensitive
  gboolean canStart = FALSE;
  if (job != 0) {
    if (state == PAUSED)
      canStart = TRUE;
    else if (!childMode && (/*state == SUCCEEDED ||*/ state == STOPPED
                            ||state == ERROR && singleUrl->resumePossible()))
      canStart = TRUE;
  }
  gtk_widget_set_sensitive(GUI::window.download_startButton, canStart);
  gtk_widget_set_sensitive(GUI::window.download_pauseButton,
                           FALSE);
  // once libcurl allows pausing: (job != 0 && state == RUNNING ? TRUE : FALSE));
  gtk_widget_set_sensitive(GUI::window.download_stopButton,
    (job != 0 && !childMode && (state == RUNNING || state == PAUSED)
     ? TRUE : FALSE));
  gtk_widget_set_sensitive(GUI::window.download_restartButton,
                           (!childMode ? TRUE : FALSE));
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
     openOutputAndResume() makes state!=PAUSED, which is what should be
     displayed */
  if (self->jobList()->isWindowOwner(self)) self->updateWindow();
}
//______________________________________________________________________

/* The job whose io ptr references us is being deleted. */
void GtkSingleUrl::job_deleted() {
  debug("job_deleted job=singleUrl=0");
  job = singleUrl = 0;
  return;
}
//______________________________________________________________________

// Called when download succeeds
void GtkSingleUrl::job_succeeded() {
  debug("job_succeeded");
  callRegularly(0);

  // Can't see the same problem as with job_failed(), but just to be safe...
  destStream.clear();
  if (!childMode) singleUrl->setDestination(0, 0, 0);
  string s;
  Progress::appendSize(&s, job->progress()->currentSize());
  progress.erase();
  status = subst(_("Download is complete - fetched %1 (%2 bytes)"),
                 s, job->progress()->currentSize());
  if (state != STOPPED) state = SUCCEEDED;
  updateWindow();
  s = subst(_("Finished - fetched %1"), s);
  job_message(s);
}
//______________________________________________________________________

/* Like job_failed() below, but don't pay attention whether
   job->resumePossible() - *never* auto-resume the download. */
void GtkSingleUrl::failedPermanently(string* message) {
  if (!childMode) singleUrl->setDestination(0, 0, 0);
  destStream.clear();
  state = ERROR;

  treeViewStatus = subst(_("<b>%1</b>"), message);
  status.swap(*message);
  updateWindow();
  gtk_tree_store_set(jobList()->store(), row(), JobList::COLUMN_STATUS,
                     treeViewStatus.c_str(), -1);
}
//______________________________________________________________________

void GtkSingleUrl::job_failed(const string& message) {
  /* Important: close the file here. Otherwise, the following can happen: 1)
     Download aborts with an error, old JobLine still displays error message,
     isn't deleted yet; its stream stays open. 2) User restarts download, a
     SECOND stream is created. It's created with ios::trunc, but that's
     ignored (under Linux) cos of the second open handle. 3) If the second
     JobLine doesn't overwrite all data from the first one, we end up with
     file contents like "data from 2nd download" + "up to a page full of null
     bytes" + "stale data from 1st download". */
  Paranoid(job != 0);
  if (!childMode) singleUrl->setDestination(0, 0, 0);
  destStream.clear();
  if (state != STOPPED) state = ERROR;
  bool resumePossible = (job != 0
                         && !childMode
                         && state == ERROR
                         && singleUrl->resumePossible());
  debug("job_failed: %1 job=%2 state=%3 resumePossible=%4",
        message, job, state, resumePossible);

  if (resumePossible) {
    treeViewStatus = subst(_("Try %1 of %2 after <b>%E3</b>"),
                           singleUrl->currentTry() + 1,
                           Job::SingleUrl::MAX_TRIES, message);
  } else {
    treeViewStatus = subst(_("<b>%E1</b>"), message);
  }
  //debug("job_failed: %1", message);
  status = message;
  if (progress.empty()) progress = _("Failed:");
  updateWindow();
  gtk_tree_store_set(jobList()->store(), row(), JobList::COLUMN_STATUS,
                     treeViewStatus.c_str(), -1);
  if (resumePossible)
    callRegularlyLater(Job::SingleUrl::RESUME_DELAY,
                       &GtkSingleUrl::startResume);
}
//______________________________________________________________________

void GtkSingleUrl::job_message(const string& message) {
  treeViewStatus = message;
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     -1);
}
//______________________________________________________________________

// Don't need this info - ignore
void GtkSingleUrl::dataSource_dataSize(uint64) {
  return;
}
//______________________________________________________________________

void GtkSingleUrl::dataSource_data(const byte* /*data*/, unsigned /*size*/,
                                  uint64 /*currentSize*/) {
  //debug("dataSource_data %1", job->progress()->currentSize());
  if (!needTicks())
    callRegularly(&GtkSingleUrl::showProgress);

  if (childMode) return;

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

//  debug("%1", *job->progress());

  if (job == 0 || state == ERROR || state == SUCCEEDED || state == STOPPED)
    return;

  Job::SingleUrl* jobx = dynamic_cast<Job::SingleUrl*>(job);
  bool resuming = (jobx != 0 && jobx->resuming());

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
  if (!resuming)
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
    if (!paused() && !resuming) {
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

  if (job == 0 || childMode || !(state == ERROR || state == SUCCEEDED
                                 || state == STOPPED)) {
    Assert(false);
    return;
  }

  // Resume download

  Paranoid(!childMode);
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
  state = RUNNING;
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
      || state == PAUSED || state == ERROR || state == SUCCEEDED
      || state == STOPPED || job->progress()->currentSize() == 0) {
    delete this;
    return;
  }

  const char* question;
  const char* buttonText;
  if (dynamic_cast<Job::SingleUrl*>(job) != 0) {
    question = _("Download in progress - really abort it?");
    buttonText = _("_Abort download");
  } else {
    question = _("Data stream active - really abort it?");
    buttonText = _("_Abort data stream");
  }
  MessageBox* m = new MessageBox(MessageBox::WARNING, MessageBox::NONE,
                                 question, 0);
  m->addStockButton("gtk-cancel", GTK_RESPONSE_CANCEL);
  m->addButton(buttonText, 0);
  m->onResponse(&afterCloseButtonClickedResponse, this);
  m->show();
  messageBox.set(m);
}

void GtkSingleUrl::afterCloseButtonClickedResponse(GtkDialog*, int r,
                                                   gpointer data) {
  if (r == GTK_RESPONSE_CANCEL) return;
  GtkSingleUrl* self = static_cast<GtkSingleUrl*>(data);
  debug("afterCloseButtonClickedResponse: deleting %1", self);
  delete self;
}
//______________________________________________________________________

void GtkSingleUrl::on_restartButton_clicked() {
  messageBox.set(0); // Close "overwrite/resume/cancel" question, if any
  if (job == 0 || state == ERROR) {
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
  debug("on_restartButton_clicked: cur=%2 speed=%1 thresh=%3", speed,
        jobProgress->currentSize(), speed * RESTART_WARNING_THRESHOLD);
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
  Assert(!childMode);

  // Kill the old download
  destStream.clear();
  if (singleUrl != 0) {
    singleUrl->setDestination(0, 0, 0);
    if (state == PAUSED) singleUrl->cont();
    singleUrl->stop();
  }

  // Start the new download
  status = _("Download was restarted - waiting...");
  treeViewStatus = _("Restarted - waiting");
  openOutputAndRun(/*true*/);
  state = RUNNING;
  progress.erase();
  updateWindow();
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     -1);
}

void GtkSingleUrl::deleteThis() {
  debug("deleteThis %1", this);
  delete this;
}

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

#include <config.h>

#include <iostream>

#include <gtk-makeimage.hh>
#include <gtk-single-url.hh>
#include <string-utf.hh>
//______________________________________________________________________

GtkMakeImage::GtkMakeImage(const string& uriStr, const string& destination)
  : progress(), status(), treeViewStatus(),
    mid(this, uriStr, destination) { }

GtkMakeImage::~GtkMakeImage() { }
//______________________________________________________________________

bool GtkMakeImage::run() {

  // Show URL as object name
  //progress.erase();
  //status = _("Waiting...");
  treeViewStatus = _("Waiting");
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     JobList::COLUMN_OBJECT, "",
                     -1);

  mid.run();

  // By default, children of this object are visible
  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(jobList()->store()), row() );
  gtk_tree_view_expand_row(jobList()->view(), path, TRUE);
  gtk_tree_path_free(path);
  return SUCCESS;
}
//______________________________________________________________________

/* User clicked on our line in the display of jobs. If the window is not
   already displaying our info, switch to it. Otherwise, cycle through the
   sub-notebook tabs of our info. */
void GtkMakeImage::selectRow() {
  if (jobList()->isWindowOwner(this)) {
    // Cycle through tabs
    GtkNotebook* notebook = GTK_NOTEBOOK(GUI::window.pageJigdo);
    int page = gtk_notebook_get_current_page(notebook);
    int npages = gtk_notebook_get_n_pages(notebook);
    // For convenience, some unused tabs are set to invisible; skip them!
    while (true) {
      ++page;
      if (page >= npages) page = 0;
      GtkWidget* entry = gtk_notebook_get_nth_page(notebook, page);
      if (GTK_WIDGET_VISIBLE(entry)) break;
    }
    cerr<<"selrowcallback "<<page<<" of "<<npages<<endl;
    gtk_notebook_set_current_page(notebook, page);
  } else {
    // Don't cycle through tabs, just switch to jigdo info in main window
    setNotebookPage(GUI::window.pageJigdo);
    jobList()->setWindowOwner(this);
  }
  updateWindow();
}
//______________________________________________________________________

bool GtkMakeImage::paused() const { return false; }
void GtkMakeImage::pause() { }
void GtkMakeImage::cont() { }
void GtkMakeImage::stop() { }

void GtkMakeImage::percentDone(uint64* cur, uint64* total) {
  *cur = 0;
  *total = 0;
}
//______________________________________________________________________

void GtkMakeImage::updateWindow() {
  if (!jobList()->isWindowOwner(this)) return;

  // URL and destination lines
  gtk_label_set_text(GTK_LABEL(GUI::window.jigdo_URL),
                     mid.jigdoUri().c_str());
  gtk_label_set_text(GTK_LABEL(GUI::window.jigdo_dest),
                     mid.tmpDir().c_str());

  // Progress and status lines
//   if (!mid.paused() && !mid.failed()) {
//     progress.erase();
//     job->progress()->appendProgress(&progress);
//   }
  gtk_label_set_text(GTK_LABEL(GUI::window.jigdo_progress),
                     progress.c_str());
  gtk_label_set_text(GTK_LABEL(GUI::window.jigdo_status),
                     status.c_str());

# if 0
  // Buttons (in)sensitive
  gtk_widget_set_sensitive(GUI::window.jigdo_startButton,
    (job != 0 && (paused() || job->succeeded()
                  || job->failed() && job->resumePossible()) ?
     TRUE : FALSE));
  gtk_widget_set_sensitive(GUI::window.jigdo_pauseButton,
    (job != 0 && !job->failed() && !job->succeeded() && !paused() ?
     TRUE : FALSE));
  gtk_widget_set_sensitive(GUI::window.jigdo_stopButton,
    (job != 0 && !job->failed() && !job->succeeded() ?
     TRUE : FALSE));
# endif
}
//______________________________________________________________________

void GtkMakeImage::job_deleted() { }
void GtkMakeImage::job_succeeded() { }

void GtkMakeImage::job_failed(string* message) {
  treeViewStatus = subst(_("<b>%E1</b>"), message);
  status.swap(*message);
  updateWindow();
  gtk_tree_store_set(jobList()->store(), row(), JobList::COLUMN_STATUS,
                     treeViewStatus.c_str(), -1);
#if DEBUG
  //#warning "TODO"
#endif
}

void GtkMakeImage::job_message(string* message) {
  treeViewStatus.swap(*message);
  gtk_tree_store_set(jobList()->store(), row(),
                     JobList::COLUMN_STATUS, treeViewStatus.c_str(),
                     -1);
}

Job::SingleUrl::IO* GtkMakeImage::makeImageDl_new(
    Job::SingleUrl* childDownload) {
# if DEBUG
  cerr << "GtkMakeImage::makeImageDl_new" << endl;
# endif
  GtkSingleUrl* child = new GtkSingleUrl(mid.jigdoUri(), childDownload);
  GUI::jobList.prepend(child, this); // New child of "this" is "child"
  bool status = child->run();
  /* NB run() cannot result in "delete child;" for child mode, so we always
     return a valid pointer here. */
  Assert(status == SUCCESS);
  return child;
}

void GtkMakeImage::makeImageDl_finished(Job::SingleUrl* childDownload) {
  GtkSingleUrl* child =
    dynamic_cast<GtkSingleUrl*>(childDownload->io().get());
  Assert(child != 0);
  delete child;
}
//______________________________________________________________________

void GtkMakeImage::on_startButton_clicked() { }
void GtkMakeImage::on_pauseButton_clicked() { }
void GtkMakeImage::on_stopButton_clicked() { }
void GtkMakeImage::on_restartButton_clicked() { }
void GtkMakeImage::on_closeButton_clicked() {
  if (jobList()->isWindowOwner(this))
    setNotebookPage(GUI::window.pageOpen);
  delete this;
}

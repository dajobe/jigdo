/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Interface to the GtkTreeView of running jobs (downloads etc), window.jobs

*/

#include <config.h>

#include <iomanip>
#include <iostream>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <jobline.hh>
#include <joblist.hh>
#include <string-utf.hh>
#include <support.hh>
#include <treeiter.hh>
//______________________________________________________________________

#if DEBUG
Logger JobList::debug("joblist");
#endif

JobList GUI::jobList;

const char* const JobList::PROGRESS_IMAGE_FILE = "progress-green.png";

GdkPixbuf* JobList::progressImage = 0;
vector<GdkPixbuf*> JobList::progressGfx;
GValue JobList::progressValue;
//______________________________________________________________________

JobList::~JobList() {
  /* Don't delete any widgets, GTK should take care of this itself when the
     window is deleted. */

  // Delete active callback, if any
  if (selectRowIdleId != 0) g_source_remove(selectRowIdleId);

  /* Delete Jobs. When deleted, the job will erase itself from the list, so
     just keep getting the first list element.
     Careful: This might be called during static finalization, even in the
     case that GTK+ was not initialized (e.g. because DISPLAY was not set).
     So only make calls to GTK+ if non-empty. */
  GtkTreeIter row;
  if (!empty()) {
    while (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store()), &row)) {
      debug("~JobList: Deleting %1", &row);
      delete get(&row);
    }
  }
  if (store()) g_object_unref(store());
}
//______________________________________________________________________

// Called from gtk-gui.cc
void JobList::postGtkInit() {

  // Set up list of downloads ("jobs") in lower part of window
  Assert(store() == 0);
  Paranoid(NR_OF_COLUMNS == 3);
  storeVal = gtk_tree_store_new(NR_OF_COLUMNS,
    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
  gtk_tree_view_set_model(view(), GTK_TREE_MODEL(store()));

  // The status column contains two renderers:
  GtkTreeViewColumn* statusColumn = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(statusColumn, _("Status"));
  // 1. Pixbuf renderer, value set via function
  GtkCellRenderer* pixbufRenderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(statusColumn, pixbufRenderer, FALSE);
  gtk_tree_view_column_set_cell_data_func(statusColumn, pixbufRenderer,
                                          &pixbufForJobLine, (gpointer)this,
                                          NULL);
  // 2. Text renderer, value set directly from COLUMN_STATUS
  GtkCellRenderer* statusRenderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_end(statusColumn, statusRenderer, TRUE);
  gtk_tree_view_column_set_attributes(statusColumn, statusRenderer, "markup",
                                      COLUMN_STATUS, NULL);

  // Set column sizes and other attributes
  gtk_tree_view_column_set_sizing(statusColumn,GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width(statusColumn, WIDTH_STATUS);
  gtk_tree_view_column_set_resizable(statusColumn, TRUE);
  gtk_tree_view_append_column(view(), statusColumn);

  GtkTreeViewColumn* objectColumn = gtk_tree_view_column_new_with_attributes(
    _("Click on lines below to display corresponding info above"),
    gtk_cell_renderer_text_new(), "text", COLUMN_OBJECT, NULL);
  gtk_tree_view_column_set_resizable(objectColumn, TRUE);
  gtk_tree_view_append_column(view(), objectColumn);

  gtk_tree_view_set_expander_column(view(), objectColumn);
  gtk_tree_view_set_reorderable(view(), FALSE);

  /* In order to make the GUI easier to understand for first-time users,
     don't display list headers until their info is really appropriate. */
  gtk_tree_view_set_headers_visible(view(), FALSE);

  // Register callback function if a row is selected
  gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(view()),
                                         &JobList::selectRowCallback,
                                         this, NULL);
  // Multiple selected rows possible
  gtk_tree_selection_set_mode(gtk_tree_view_get_selection(view()),
                              GTK_SELECTION_MULTIPLE);
  //____________________

  pixbufForJobLine_init();
}
//______________________________________________________________________

void JobList::pixbufForJobLine_init() {
  if (progressImage != 0) return;

  progressImage = create_pixbuf(PROGRESS_IMAGE_FILE);
  if (progressImage == 0) return;

  // Init value
  memset(&progressValue, 0, sizeof(progressValue));
  g_value_init(&progressValue, G_TYPE_FROM_INSTANCE(progressImage));

  int width = gdk_pixbuf_get_width(progressImage);
  int height = gdk_pixbuf_get_height(progressImage);
  // height must be evenly divisible by PROGRESS_SUBDIV
  Assert(height % PROGRESS_SUBDIV == 0);
  int subHeight = height / PROGRESS_SUBDIV;

  for (int y = 0; y < height; y += subHeight) {
    GdkPixbuf* sub = gdk_pixbuf_new_subpixbuf(progressImage, 0, y,
                                              width, subHeight);
    progressGfx.push_back(sub);
  }
}
//________________________________________

void JobList::pixbufForJobLine(GtkTreeViewColumn*, GtkCellRenderer* cell,
                               GtkTreeModel*, GtkTreeIter* iter,
                               gpointer data) {
  JobList* self = static_cast<JobList*>(data);
  if (progressImage == 0) return;
  JobLine* j = self->get(iter);
  uint64 cur, total;
  j->percentDone(&cur, &total);

  unsigned subNr;
  if (cur == 0 && total == 0) {
    subNr = 0;
  } else if (cur >= total) {
    subNr = PROGRESS_SUBDIV - 1;
  } else {
    subNr = cur * (PROGRESS_SUBDIV - 1) / total;
  }

  if (subNr >= progressGfx.size() || progressGfx[subNr] == 0) return;

  g_value_set_object(&progressValue, (gpointer)progressGfx[subNr]);
  g_object_set_property(G_OBJECT(cell), "pixbuf", &progressValue);
}
//______________________________________________________________________

void JobList::prepend(JobLine* j, JobLine* parent) {
  Paranoid(j != 0);
  GtkTreeIter* parentIter = (parent == 0 ? NULL : parent->row());
  GtkAdjustment* scrollbar = gtk_scrolled_window_get_vadjustment(
      GTK_SCROLLED_WINDOW(GUI::window.progressScroll));
  double pos = gtk_adjustment_get_value(scrollbar);
  gtk_tree_store_prepend(store(), j->row(), parentIter); // Add empty row
  gtk_tree_store_set(store(), j->row(), COLUMN_DATA, j, -1);
  if (pos < 0.1) {
    // We were at top, so scroll to top again
    gtk_tree_view_scroll_to_point(view(), -1, 0); // Actually, has no effect
    g_idle_add((GSourceFunc)progressScrollToTop, view());
  }
  if (++sizeVal == 1) gtk_tree_view_set_headers_visible(view(), TRUE);
  ++entryCountVal;
  j->jobVec = this;
}

gboolean JobList::progressScrollToTop(gpointer view) {
  gtk_tree_view_scroll_to_point(GTK_TREE_VIEW(view), -1, 0);
  return FALSE;
}

void JobList::append(JobLine* j, JobLine* parent) {
  Paranoid(j != 0);
  GtkTreeIter* parentIter = (parent == 0 ? NULL : parent->row());
  gtk_tree_store_append(store(), j->row(), parentIter); // Add empty row
  gtk_tree_store_set(store(), j->row(), COLUMN_DATA, j, -1);
  if (++sizeVal == 1) gtk_tree_view_set_headers_visible(view(), TRUE);
  ++entryCountVal;
  j->jobVec = this;
}

void JobList::erase(GtkTreeIter* row) {
  // Delete entry
  JobLine* j = get(row);
  gtk_tree_store_remove(store(), row);
  if (j != 0) {
    //delete j;
    Paranoid(entryCountVal > 0);
    --entryCountVal;
  }
  Paranoid(sizeVal > 0);
  --sizeVal;
}

void JobList::makeRowBlank(GtkTreeIter* row) {
  // Decrease number of data entries
  JobLine* j = get(row);
  if (j != 0) {
    Paranoid(entryCountVal > 0);
    --entryCountVal;
  }

  // Set row blank, clear data field
  Paranoid(NR_OF_COLUMNS == 3);
  gtk_tree_store_set(store(), row, 0, 0, 1, 0, 2, 0, -1);
  // Deselect row
  GtkTreeSelection* selection = gtk_tree_view_get_selection(view());
  gtk_tree_selection_unselect_iter(selection, row);
}
//______________________________________________________________________

#if DEBUG
void JobList::assertValid() const {
  size_type realEntryCount = 0, realSize = 0;
  int realNeedTicks = 0;
  GtkTreeIter row;
  gboolean ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store()), &row);
  while (ok) {
    ++realSize;
    JobLine* j = get(&row);
    if (j != 0) {
      ++realEntryCount;
      if (j->needTicks()) ++realNeedTicks;
    }
    ok = gtk_tree_model_iter_next_depth(GTK_TREE_MODEL(store()), &row);
  }
  if (realSize != size())
    debug("realSize=%1 size()=%2", realSize, size());
  Assert(realEntryCount == entryCount());
  Assert(realSize == size());
  Assert(entryCount() <= size());
  Assert(realNeedTicks == needTicks);
}
#endif
//______________________________________________________________________

gint JobList::timeoutCallback(gpointer jobList) {
  JobList* self = static_cast<JobList*>(jobList);

  if (DEBUG) self->assertValid();

  // For each JobLine entry in the list that needs it, make a call to tick()
  GtkTreeIter row;
  gboolean ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(self->store()),
                                              &row);
  while (ok) {
    JobLine* j = self->get(&row);
    ok = gtk_tree_model_iter_next_depth(GTK_TREE_MODEL(self->store()), &row);
    // Yargh! This syntax took me 30 minutes to get right:
    if (j != 0 && j->needTicks()) (j->*(j->tick))();
  }
  return TRUE;
}
//______________________________________________________________________

/* Called when the user clicks on a line in the job list. Simply passes the
   click on to the object whose line was clicked on, by calling its
   selectRow() virtual method.

   This is often called >1 times for just one click. Thus, register a
   callback which is executed at the next iteration of the main loop, and
   only call selectRow() if the callback isn't yet registered. */
gboolean JobList::selectRowCallback(GtkTreeSelection* /*selection*/,
                                    GtkTreeModel* model,
                                    GtkTreePath* path,
                                    gboolean path_currently_selected,
                                    gpointer data) {
  if (path_currently_selected) return TRUE;

  JobList* self = static_cast<JobList*>(data);
  if (self->selectRowIdleId != 0)
    return TRUE; // Callback already pending - do nothing
  self->selectRowIdleId = g_idle_add(&selectRowIdle, self);

  debug("selectRowCallback");
  GtkTreeIter row;
  bool ok = gtk_tree_model_get_iter(model, &row, path);
  Assert(ok);
  JobLine* job = self->get(&row);
  if (job != 0) job->selectRow();
  return TRUE;
}

gboolean JobList::selectRowIdle(gpointer data) {
  debug("selectRowIdle");
  JobList* self = static_cast<JobList*>(data);
  self->selectRowIdleId = 0;
  return FALSE; // "Don't call me again"
}

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Algorithm for depth-first traversal of the objects in a GtkTreeModel, the
  parent is visited before its children.

  Usage:

  GtkTreeIter row;
  gboolean ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store()), &row);
  while (ok) {
    // Do something with "row"
    ok = gtk_tree_model_iter_next_depth(GTK_TREE_MODEL(store()), &row);
  }

*/

#ifndef GTK_TREEITER_HH
#define GTK_TREEITER_HH

#include <config.h>
#include <gtk/gtk.h>

bool gtk_tree_model_iter_next_depth(GtkTreeModel* model, GtkTreeIter* iter);

#endif

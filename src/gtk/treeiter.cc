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

*/

#include <treeiter.hh>
//______________________________________________________________________

bool gtk_tree_model_iter_next_depth(GtkTreeModel* model, GtkTreeIter* iter) {
  GtkTreeIter x;
  if (gtk_tree_model_iter_children(model, &x, iter) == TRUE) {
    // Descend to first child
    *iter = x;
    return true;
  }

  while (true) {
    x = *iter;
    // Try to move to right sibling
    if (gtk_tree_model_iter_next(model, iter) == TRUE) return true;
    /* No right sibling, so step upward to parent, then (looping back) move
       to its right sibling. */
    if (gtk_tree_model_iter_parent(model, iter, &x) == FALSE) return false;
  }
}

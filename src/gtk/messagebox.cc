/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Display an error box with a message and standard or user-supplied buttons.

*/

#include <config.h>

#include <debug.hh>
#include <gui.hh>
#include <log.hh>
#include <messagebox.hh>
#include <string-utf.hh>
#include <support.hh>
//______________________________________________________________________

DEBUG_UNIT("messagebox")

const char* const MessageBox::MESSAGE = "gtk-dialog-info";
const char* const MessageBox::INFO = "gtk-dialog-info";
const char* const MessageBox::WARNING = "gtk-dialog-warning";
const char* const MessageBox::QUESTION = "gtk-dialog-question";
const char* const MessageBox::ERROR = "gtk-dialog-error";
//______________________________________________________________________

void MessageBox::init(const char* type, int buttons,
                      const char* heading, const char* message) {
  escapeButton = 0;
  ref = 0;

  // Create dialog layout
  dialog = gtk_dialog_new();
  const char* title = "Jigsaw Download";
# if DEBUG
  static unsigned bark = 0;
  if ((++bark & 3) == 0) {
    if (strcmp(type, MESSAGE) == 0 || strcmp(type, INFO) == 0)
      title = "Wicked message with subliminal meaning";
    else if (strcmp(type, WARNING) == 0)
      title = "Clueless user alert";
    else if (strcmp(type, QUESTION) == 0)
      title = "Innocent-looking question";
    else if (strcmp(type, ERROR) == 0)
      title = "Sigh, I _knew_ you were going to do that";
  }
# endif
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
  GtkWidget* vbox = GTK_DIALOG(dialog)->vbox;
  gtk_box_set_spacing(GTK_BOX(vbox), 12);
  gtk_widget_show(vbox);
  GtkWidget* hbox = gtk_hbox_new(FALSE, 12);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
  GtkWidget* image = gtk_image_new_from_stock(type, GTK_ICON_SIZE_DIALOG);
  gtk_widget_show(image);
  gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
  gtk_misc_set_alignment(GTK_MISC(image), 0.0, 0.0);

  // Add message label
  GtkWidget* msg;
  if (heading == 0) {
    label = message;
  } else {
    label = "<span weight=\"bold\" size=\"larger\">";
    label += heading;
    label += "</span>";
    if (message != 0 && *message != '\0') {
      label += "\n\n";
      label += message;
    }
  }
  msg = gtk_label_new(label.c_str());
  gtk_label_set_use_markup(GTK_LABEL(msg), TRUE);
  gtk_label_set_selectable(GTK_LABEL(msg), TRUE);
  gtk_widget_show(msg);
  gtk_box_pack_end(GTK_BOX(hbox), msg, FALSE, FALSE, 0);
  gtk_label_set_line_wrap(GTK_LABEL(msg), TRUE);

  // Add buttons
  gtk_widget_show(GTK_DIALOG(dialog)->action_area);
  gtk_button_box_set_layout(GTK_BUTTON_BOX(GTK_DIALOG(dialog)->action_area),
                            GTK_BUTTONBOX_END);
  if (buttons & HELP)
    addStockButton("gtk-help", GTK_RESPONSE_HELP);
  if (buttons & CANCEL)
    addStockButton("gtk-cancel", GTK_RESPONSE_CANCEL);
  if (buttons & CLOSE)
    addStockButton("gtk-close", GTK_RESPONSE_CLOSE);
  if (buttons & OK)
    addStockButton("gtk-ok", GTK_RESPONSE_OK);

  g_signal_connect(G_OBJECT(dialog), "destroy", G_CALLBACK(destroyHandler),
                   (gpointer)this);

  /* "close" is emitted when the user presses Escape. The default behaviour
     of GTK+ 2.2 is silly: If the dialog has 1 button, nothing happens, if it
     has >1, the dialog is destroyed. To avoid lots of extra code, we prevent
     it from being destroyed and simulate a button click instead. */
  g_signal_connect(G_OBJECT(dialog), "close", G_CALLBACK(closeHandler),
                   (gpointer)this);
}
//______________________________________________________________________

MessageBox* MessageBox::show_noAutoClose() {
  if (escapeButton == 0) {
    /* No "Cancel" button was added to this MessageBox. If the dialog only
       has one button, instead simulate a click on that button when the user
       presses Escape. */
    GtkContainer* c = GTK_CONTAINER(GTK_DIALOG(dialog)->action_area);
    GList* l = gtk_container_get_children(c);
    if (l != 0 && g_list_nth(l, 1) == NULL) {
      debug("Show: single button, not cancel");
      escapeButton = GTK_WIDGET(g_list_first(l)->data);
    }
  }
  gtk_widget_show(dialog);
  return this;
}
//______________________________________________________________________

MessageBox::~MessageBox() {
  debug("~MessageBox this=%1 ref=%2", this, ref);
  if (ref != 0) {
    Assert(ref->get() == this);
    ref->messageBox = 0;
  }
  if (dialog != 0) {
    GtkWidget* tmp = dialog;
    dialog = 0;
    gtk_widget_destroy(tmp);
  }
}
//______________________________________________________________________

GtkWidget* MessageBox::addButton(const char* buttonText,
                                 int response) {
  GtkWidget* b = gtk_button_new_with_mnemonic(buttonText);
  gtk_widget_show(b);
  gtk_dialog_add_action_widget(GTK_DIALOG(dialog), b, response);
  GTK_WIDGET_SET_FLAGS(b, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(b);
  if (response == GTK_RESPONSE_CANCEL && escapeButton == 0) escapeButton = b;
  return b;
}
//______________________________________________________________________

GtkWidget* MessageBox::addStockButton(const char* buttonType,
                                      int response) {
  GtkWidget* b = gtk_button_new_from_stock(buttonType);
  gtk_widget_show(b);
  gtk_dialog_add_action_widget(GTK_DIALOG(dialog), b, response);
  GTK_WIDGET_SET_FLAGS(b, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(b);
  if (response == GTK_RESPONSE_CANCEL) escapeButton = b;
  return b;
}
//______________________________________________________________________

void MessageBox::destroyHandler(GtkDialog*, MessageBox* m) {
  debug("destroyHandler this=%1 dialog=%2", m, m->dialog);
  if (m->dialog != 0) delete m;
}

void MessageBox::autocloseHandler(GtkDialog*, int r, MessageBox* m) {
  debug("autoCloseHandler this=%1 dialog=%2 response=%3", m, m->dialog, r);
  if (r != GTK_RESPONSE_HELP) delete m;
}

void MessageBox::closeHandler(GtkDialog* dialog, MessageBox* m) {
  g_signal_stop_emission_by_name(dialog, "close");
  if (m->escapeButton != 0) {
    debug("closeHandler click");
    g_signal_emit_by_name(G_OBJECT(m->escapeButton), "clicked",
                          m->escapeButton);
  }
}

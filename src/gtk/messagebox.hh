/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Display an error box with a message and standard or user-supplied buttons.

*/

#ifndef GTK_MESSAGEBOX_HH
#define GTK_MESSAGEBOX_HH

#include <config.h>

#include <string>
#include <gtk/gtk.h>

#include <debug.hh>
#include <nocopy.hh>

#ifdef MessageBox
#  undef MessageBox // fsck Windows!!
#endif
#ifdef ERROR
#  undef ERROR
#endif
//______________________________________________________________________

class MessageBox : NoCopy {
public:

  class Ref;
  friend class Ref;

  enum {
    NONE       = 0,
    HELP       = 1 << 0,
    OK         = 1 << 1,
    CLOSE      = 1 << 2,
    CANCEL     = 1 << 3
  };

  static const char* const MESSAGE;
  static const char* const INFO;
  static const char* const WARNING;
  static const char* const QUESTION;
  static const char* const ERROR;

  /** Displays message in a GtkMessageDialog - when the user clicks the OK
      button, the window is closed again. Any number of independent error
      boxes can be open at the same time.

      For non-standard buttons, use GTK_BUTTONS_NONE, then
      addButton().

      heading and message must be valid UTF-8. Markup is not escaped, do this
      yourself if necessary. message is optional.

      @param type    Type of icon to display
      @param buttons Or'ed together: HELP, OK, CLOSE, CANCEL. HELP is special
                     in that the button is added at the left side of the
                     dialog. OK is special because a default signal handler
                     is added which closes the dialog.
      @param heading Error text to be printed in big font at top of dialog,
                     or null if only message is to be displayed.
      @param message Main error message
  */
  inline MessageBox(const char* type, int buttons, const char* heading,
                    const char* message = 0);
  /** As above, but with string& instead of char* */
  inline MessageBox(const char* type, int buttons, const string& heading,
                    const char* message);
  /** As above, but with string& instead of char* */
  inline MessageBox(const char* type, int buttons, const char* heading,
                    const string& message);
  /** As above, but with string& instead of char* */
  inline MessageBox(const char* type, int buttons, const string& heading);
  /** As above, but with string& instead of char* */
  inline MessageBox(const char* type, int buttons, const string& heading,
                    const string& message);

  ~MessageBox();

  /** Open the dialog box. (This isn't done in the ctor because if you add
      your own buttons after opening it, its size changes, which may cause
      the window manager to display it partially off-screen.) Also registers
      a callback which closes the window once a button is pressed, *except*
      when that button causes a response GTK_RESPONSE_HELP
      @return "this" */
  inline MessageBox* show();
  /** As above, but don't close when a button is pressed. */
  MessageBox* show_noAutoClose();

  /** Add a text button to the dialog.
      @return The new button */
  GtkWidget* addButton(const char* buttonText, int response);
  inline GtkWidget* addButton(const char* buttonText,
                              GtkResponseType response);
  /** Add a standard button to the dialog.
      @param buttonType e.g. "gtk-cancel"
      @param response e.g. GTK_RESPONSE_CANCEL, or a positive value of your
      choice
      @return The new button */
  GtkWidget* addStockButton(const char* buttonType, int response);
  inline GtkWidget* addStockButton(const char* buttonType,
                                   GtkResponseType response);

  typedef void (*ResponseHandler)(GtkDialog*, int, gpointer);
  /** You must take care yourself to be delivered signals when the user
      clicks on a button. The MessageBox object is deleted when the dialog is
      destroyed - usually, this happens automatically, but it won't happen
      for a click on a "Help" button and if you used show_noAutoClose()
      instead of show().
      @return "this" */
  inline MessageBox* onResponse(ResponseHandler handler, gpointer data);

private:
  void init(const char* type, int buttons, const char* heading,
            const char* message);

  static void destroyHandler(GtkDialog*, MessageBox* m);
  static void autocloseHandler(GtkDialog*, int, MessageBox* m);
  static void closeHandler(GtkDialog*, MessageBox* m);

  GtkWidget* dialog;
  // The button to simulate a click on if Escape is pressed, or null
  GtkWidget* escapeButton;
  Ref* ref;
  string label;
};
//______________________________________________________________________


/** Ref object to store reference to a MessageBox in. If the Ref object
    already contained a reference, that MessageBox is deleted (i.e. closed on
    screen). If the MessageBox is deleted, it sets its reference to null.
    Useful to ensure that only one MessageBox is ever open for a certain Ref
    instance. */
class MessageBox::Ref {
  friend class MessageBox;
public:
  Ref() : messageBox(0) { }
  inline ~Ref();
  inline void set(MessageBox* m);
  MessageBox* get() const { return messageBox; }
private:
  // Don't copy
  inline MessageBox& operator=(const MessageBox&);

  MessageBox* messageBox;
};
//______________________________________________________________________

MessageBox::MessageBox(const char* type, int buttons, const char* heading,
  const char* message) { init(type, buttons, heading, message); }
MessageBox::MessageBox(const char* type, int buttons, const string& heading,
  const char* message) { init(type, buttons, heading.c_str(), message); }
MessageBox::MessageBox(const char* type, int buttons, const char* heading,
  const string& message) { init(type, buttons, heading, message.c_str()); }
MessageBox::MessageBox(const char* type, int buttons,
  const string& heading) { init(type, buttons, heading.c_str(), 0); }
MessageBox::MessageBox(const char* type, int buttons, const string& heading,
                       const string& message) {
  init(type, buttons, heading.c_str(), message.c_str());
}

GtkWidget* MessageBox::addButton(const char* buttonText,
                                 GtkResponseType response) {
  return addButton(buttonText, static_cast<int>(response));
}

GtkWidget* MessageBox::addStockButton(const char* buttonType,
                                      GtkResponseType response) {
  return addStockButton(buttonType, static_cast<int>(response));
}

MessageBox* MessageBox::onResponse(ResponseHandler handler, gpointer data) {
  g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(handler), data);
  return this;
}

MessageBox* MessageBox::show() {
  g_signal_connect(G_OBJECT(dialog), "response",
                   G_CALLBACK(&autocloseHandler), (gpointer)this);
  return show_noAutoClose();
}

MessageBox::Ref::~Ref() {
  set(0);
}

void MessageBox::Ref::set(MessageBox* m) {
  if (messageBox == m) return;
  if (messageBox != 0) {
    messageBox->ref = 0;
    delete messageBox;
  }
  if (m != 0) {
    Assert(m->ref == 0);
    m->ref = this;
  }
  messageBox = m;
}

#endif

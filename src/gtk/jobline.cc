/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  One line in a JobList, in the lower part of the jigdo GUI window

*/

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <config.h>
#include <gtk-makeimage.hh>
#include <gtk-single-url.hh>
#include <jobline.hh>
#include <messagebox.hh>
#include <string-utf.hh>
//______________________________________________________________________

DEBUG_TO(JobList::debug)

namespace {

  /* Returns true if last characters of uri are the same as ext.
     Cannot use string::compare because of incompatibility between
     GCC2.95/3.0 */
  inline bool compareEnd(const string& uri, const char* ext) {
    static const unsigned extLen = strlen(ext);
    if (uri.size() < extLen) return false;
    unsigned pos = uri.size() - extLen;
    for (unsigned i = 0; i < extLen; ++i)
      if (ext[i] != uri[pos + i]) return false;
    return true;
  }

}
//______________________________________________________________________

void JobLine::create(const char* uri, const char* dest) {

  if (*uri == '\0') {
    MessageBox* m = new MessageBox(MessageBox::INFO, MessageBox::OK,
      _("Field for source URL/filename is empty"),
      _("Please enter an \"http\" or \"ftp\" URL to download, or the "
        "name/URL of a <tt>.jigdo</tt> file to process."));
    m->show();
    return;
  }

  /* Removing trailing '\' is necessary on Windows. Also, makeimagedl expects
     no trailing DIRSEP. */
  string destination(dest);
  unsigned destLen = destination.length();
  while (destLen > 1 && destination[destLen - 1] == DIRSEP) --destLen;
  destination.resize(destLen);

  struct stat fileInfo;
  int statDest = stat(destination.c_str(), &fileInfo);
  bool destIsDir = S_ISDIR(fileInfo.st_mode);

  if (statDest != 0 && errno != ENOENT) {
    // destination is present, but error accessing it
    MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK,
      _("Error accessing destination"),
      subst(_("The destination `%LE1' is present, but cannot be accessed: "
              "%LE2"), destination, strerror(errno)));
    m->show();
    return;
  }

  // Also create parent object
  unsigned lastDirSep = destination.rfind(DIRSEP);
  if (lastDirSep == 0) lastDirSep = 1; // Parent of "/tmp" is "/" not ""
  string destParent(destination, 0, lastDirSep); // can be ==destination
  int statDestParent = stat(destParent.c_str(), &fileInfo);
  if (statDestParent != 0) {
    MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK,
      _("Error accessing directory to save to"),
      subst(_("The destination `%LE1' cannot be accessed: %LE2"),
            destination, strerror(errno)));
    m->show();
    return;
  }

  /* We perform a regular download if the destination is either a filename
     (as opposed to a dir name), or the URI is not a jigdo file. */
  string uriStr(uri);
  if ((statDest != 0 && errno == ENOENT) // destination does not exist
      || (statDest == 0 && !destIsDir) // exists as file
      || !compareEnd(uriStr, ".jigdo")) { // URI end not ".jigdo"
    if (statDest == 0 && destIsDir) {
      // Append filename from source to directory name
      unsigned lastDirSep = uriStr.rfind('/');
      destination += DIRSEP;
      /* If URL ends with /, use index.html as filename. DON'T try to use any
         name supplied by the server during a HTTP redirect - a malicious
         server could trick us into overwriting files! */
      if (lastDirSep == uriStr.length() - 1)
        destination.append("index.html");
      else
        destination.append(uriStr, lastDirSep + 1, string::npos);
    }
    GtkSingleUrl* result = new GtkSingleUrl(uriStr, destination);
    GUI::jobList.prepend(result);
    result->run();
    // NB result may already have deleted itself at this point
    return;
  }

  static bool dom = false;
  if (!dom) {
    dom = true;
    MessageBox* m = new MessageBox(MessageBox::INFO, MessageBox::NONE,
        "values of Î² will give rise to dom!",
        "Processing of .jigdo files is not yet implemented - only "
        "downloads of single files (e.g. <tt>.iso</tt> images) work.\n"
        "jigdo might crash any minute now - don't complain, I'm working on "
        "it! :-)\n"
        "Please use <tt>jigdo-lite</tt> to process .jigdo URLs.");
    m->addButton(_("_Cool!"), 0);
    m->addButton(_("_Awesome!"), 0);
    m->addButton(_("_Fantastic!"), GTK_RESPONSE_CANCEL);
    m->show();
  }

  // Not a regular download => a .jigdo download
  // destination is a directory, uriStr ends in ".jigdo"
  GtkMakeImage* result = new GtkMakeImage(uriStr, destination);
  GUI::jobList.prepend(result);
  try {
    result->run();
  } catch (Error e) {
    MessageBox* m = new MessageBox(MessageBox::ERROR, MessageBox::OK, 0,
                                   subst("%E1", e.message));
    m->show();
    delete result;
    return;
  }
  return;
}
//______________________________________________________________________

JobLine::~JobLine() {
  if (needTicks()) jobList()->unregisterTicks();
  jobList()->erase(row());
}
//______________________________________________________________________

void JobLine::waitTick() {
  if (--waitCountdown == 0) callRegularly(waitDestination);
  debug("waitCountdown=%1", waitCountdown);
}

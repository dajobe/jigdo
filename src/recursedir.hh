/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Create recursive directory listing, avoiding symlink loops

*/

#ifndef RECURSEDIR_HH
#define RECURSEDIR_HH

#include <config.h>

#include <dirent.hh>

#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <stack>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <debug.hh>
#include <recursedir.fh>
//______________________________________________________________________

/** Errors which occur during RecurseDir's work */
struct RecurseError : Error {
  explicit RecurseError(const string& m) : Error(m) { }
  explicit RecurseError(const char* m) : Error(m) { }
};

/** A filename generator; feed it names of single files or
    directories, it will output them, recursing depth-first through
    directories. It does follow symlinks, but will not output more
    than one name for an inode, and avoid symlink loops. If an inode
    can be reached both through its "normal" name and through symlinks
    during recursion into one directory, it is guaranteed that the
    normal name will be listed. */
class RecurseDir {
public:
  RecurseDir() : curDir(), recurseStack(), objects(), objectsFrom(),
                 fileList(), listStream(0) { }
  inline ~RecurseDir();

  /// Provide single file/directory name to output or recurse into
  void addFile(const char* name) { objects.push(string(name)); }
  void addFile(const string& name) { objects.push(name); }
  /// To read filenames from stdin, pass an empty string
  void addFilesFrom(const char* name) { objectsFrom.push(string(name)); }
  void addFilesFrom(const string& name) { objectsFrom.push(name); }

  /// Are there no filename sources present at all?
  bool empty() const { return objects.empty() && objectsFrom.empty(); }

  /** Returns FAILURE if no more names. After a RecurseError has been
      thrown, it is no problem to continue using the object.
      @param result Returned filename (output only)
      @param fileInfo getName() must call stat() for any filename it
      returns, in order to determine whether it is a directory. If the
      information returned by stat() is useful for you, supply your
      own struct stat for getName() to use. */
  bool getName(string& result, struct stat* fileInfo = 0)
      throw(RecurseError, bad_alloc);

  /** Flush list of "already visited device/inode pairs", which would
      otherwise be skipped during further scanning of filenames. */

private:
  // Insert fileInfo in beenThere, return true if it was already there
  inline bool alreadyVisited(const struct stat* fileInfo);
  // For recording device/inode of objects already visited
  struct DevIno {
    DevIno(const struct stat* s) : dev(s->st_dev), ino(s->st_ino) { }
    bool operator<(const DevIno& d) const {
      return (ino < d.ino) || (ino == d.ino && dev < d.dev); }
    dev_t dev; ino_t ino;
  };
  // One stack entry (recursion level) when recursing into directories
  struct Level {
    Level(DIR* d, size_t l) : dir(d), dirNameLen(l) { }
    void close() { if (dir == 0) return; closedir(dir); dir = 0; }
    DIR* dir; // Handle for readdir()
    size_t dirNameLen; // Length to shorten curDir to to get this dir's name
  };
  string curDir;
  stack<Level> recurseStack;

  inline bool RecurseDir::getNextObjectName(string& result)
    throw(RecurseError);
  queue<string> objects; // Queue of filenames to output/dirs to recurse into
  queue<string> objectsFrom; // Files containing filenames
  ifstream fileList; // Was head of objectsFrom once, now has been opened
  istream* listStream; // null if not open, else &fileList, or &cin
# if HAVE_LSTAT
  set<DevIno> beenThere; // Already visited inodes, for loop prevention
# endif
};
//______________________________________________________________________

#if HAVE_LSTAT
  inline bool isSymlink(struct stat* buf) { return S_ISLNK(buf->st_mode); }
  bool RecurseDir::alreadyVisited(const struct stat* fileInfo) {
    pair<set<DevIno>::iterator, bool>
      ins = beenThere.insert(DevIno(fileInfo));
    return !ins.second;
  }
#else
  inline int lstat(const char* filename, struct stat* buf) {
    return stat(filename, buf);
  }
  inline bool isSymlink(struct stat*) { return false; }
  bool RecurseDir::alreadyVisited(const struct stat*) { return false; }
#endif
//____________________

RecurseDir::~RecurseDir() {
  if (listStream != 0 && listStream != &cin) fileList.close();
  while (!recurseStack.empty()) {
    recurseStack.top().close();
    recurseStack.pop();
  }
}

#endif

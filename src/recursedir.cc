/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Create recursive directory listing, avoiding symlink loops

  Problem: The user might use "find -f", which does not follow
  symlinks, but they might also preload the jigdo cache with
  "jigdo-file scan", which does. Thus, if we were to recurse into
  symlinks to diretories before recursing into the real directories
  themselves, the files in those directories would only be entered
  into the cache with a name via a symlink, not their real name.

  Queries to the cache use the filename, so when the user used "find
  -f", there will be a query for blah/foo/somefile, whereas if
  RecurseDir didn't handle symlinks specially, only be a cache entry
  for blah/symlink-to-foo/somefile would be present, causing the cache
  lookup to fail.

  This goes into great contortions in order to first access
  non-symlink objects and then symlinks.

*/

#include <config.h>

#include <recursedir.hh>

#include <iostream>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string.hh>
//______________________________________________________________________

namespace {

  void throw_RecurseError_forObject(const string& name) {
    string err = subst(_("Skipping object `%1' (%2)"),
                       name, strerror(errno));
    throw RecurseError(err);
  }

  void throw_RecurseError_forDir(const string& name) {
    string err = subst(_("Error reading from directory `%1' (%2)"),
                       name, strerror(errno));
    throw RecurseError(err);
  }

  //______________________________________________________________________

  /* Assign the next object name to result. Returns FAILURE if no more
     names available. Note: An object name is immediately removed from
     the start of "objects" when it is copied to "result". The name of
     a file from which filenames are read, in contrast, stays at the
     head of "objectsFrom" until the file is closed, to allow for
     proper error messages to be generated. */
  bool RecurseDir::getNextObjectName(string& result) throw(RecurseError) {
    while (true) {

      // Try to assign the name of an object (name or dir) to result
      if (!objects.empty()) {
        // Out of single list of object names
        result = objects.front();
        //cerr << "getNextObjectName: single " << result << endl;
        objects.pop();
        return SUCCESS;
        //________________________________________

      } else if (listStream != 0) {
        // Read line from open file or stdin
        Paranoid(listStream == &cin || fileList.is_open());
        if (listStream->eof()) {
          if (listStream != &cin) fileList.close();
          objectsFrom.pop();
          listStream = 0;
          continue;
        }
        if (!listStream->good()) {
          string err;
          if (listStream == &cin) {
            err = subst(
                _("Error reading filenames from standard input (%1)"),
                strerror(errno));
          } else {
            err = subst(_("Error reading filenames from `%1' (%2)"),
                        objectsFrom.front(), strerror(errno));
            fileList.close();
          }
          objectsFrom.pop();
          listStream = 0;
          throw RecurseError(err);
        }
        getline(*listStream, result);
        /* An empty line terminates the list of files - this allows both
           the list and the image data to be fed to stdin with jigdo */
        if (result.empty()) {
          if (listStream != &cin) fileList.close();
          objectsFrom.pop();
          listStream = 0;
          continue;
        }
        return SUCCESS;
        //________________________________________

      } else if (!objectsFrom.empty()) {
        // Open new file to read lines from
        //cerr << "getNextObjectName: opening list of filenames"<< endl;
        Paranoid(listStream == 0);
        //cerr<<"getNextObjectName: open new " << objectsFrom.front()<< endl;
        if (objectsFrom.front().size() == 0) {
          listStream = &cin;
          continue;
        }
        fileList.open(objectsFrom.front().c_str(), ios::binary);
        listStream = &fileList;
        continue;
        //________________________________________

      } else {
        // Out of luck - no more filenames left to supply to caller
        return FAILURE;
      }
    }
  }

} // local namespace
//________________________________________

bool RecurseDir::getName(string& result, struct stat* fileInfo)
    throw(RecurseError, bad_alloc) {
  static struct stat fInfo;
  if (fileInfo == 0) fileInfo = &fInfo;

  while (true) {

    //cerr << "Recurse: stack size " << recurseStack.size() << ", `"
    //     << curDir << "'" << endl;
    if (!recurseStack.empty()) {
      // Continue recursing through directories
      Level& level = recurseStack.top();
      struct dirent* entry;
      while (true) {
        entry = readdir(level.dir);
#       if UNIX || WINDOWS
        if (entry == 0) break;
        const char* n = entry->d_name;
        if (n[0] == '.' && (n[1] == 0 || (n[1] == '.' && n[2] == 0))) {
          //cerr << "Recurse: Skip `" << n << "'" << endl;
          continue;
        }
#       endif
        break;
      }
      if (entry == 0) {
        // End-of-directory reached, continue one dir level up
        //cerr << "Recurse: End of dir `" << curDir << "'" << endl;
        level.close();
        recurseStack.pop();
        if (!recurseStack.empty())
          curDir.erase(recurseStack.top().dirNameLen); // Remove leafname
        continue;
      }
      //____________________

      // Valid object name was read from directory
      result = curDir;
      result += entry->d_name;
      if (lstat(result.c_str(), fileInfo) != 0)
        throw_RecurseError_forObject(result);
      if (isSymlink(fileInfo)) {
        // Do not handle object now, push at end of queue
        objects.push(result);
        //cerr << "Recurse: Symlink `" << result << "' pushed" << endl;
        continue;
      }
      //____________________

      // Skip object if this inode has already been visited
      if (alreadyVisited(fileInfo)) {
#       if DEBUG
        //cerr << "Recurse: `" << result << "' ignored" << endl;
#       endif
        continue;
      }

      if (!S_ISDIR(fileInfo->st_mode)) {
        // Object is regular file (or device/other non-directory)
        return SUCCESS;
      }

      // Object is a directory - recurse
      //cerr << "Recurse: into `" << result << "'" << endl;
      DIR* dir = opendir(result.c_str());
      if (dir == 0)
        throw_RecurseError_forDir(result);
      curDir += entry->d_name;
      curDir += DIRSEP;
      recurseStack.push(Level(dir, curDir.length()));
      continue;

    } // endif (!recurseStack.empty())
    //________________________________________

    // Finished recursing - any more input objects?
    if (getNextObjectName(result)) return FAILURE;

#   if WINDOWS
    /* Allow trailing '\' in input args, by removing it before
       passing it on - but don't remove for "C:\" or "\". */
    size_t len = result.length();
    if (result[len - 1] == DIRSEP
        && !(len == 3 && isalpha(result[0]) && result[1] == ':')
        && !(len == 1))
      result.erase(len - 1);
#   endif

    /* 'result' now holds the name of a file (=> we output it) or a
       directory (=> we recurse into it). There is no distinction
       between symlinks and non-symlinks here. */
    if (stat(result.c_str(), fileInfo) != 0)
      throw_RecurseError_forObject(result);
    if (alreadyVisited(fileInfo)) {
#     if DEBUG
      cerr << "Recurse: arg `" << result << "' ignored" << endl;
#     endif
      continue;
    }
    //____________________

    if (!S_ISDIR(fileInfo->st_mode)) {
      // Object is regular file (or device/whatever - just output)
      return SUCCESS;
    }

    // Object is directory - recurse
    DIR* dir = opendir(result.c_str());
    if (dir == 0)
      throw_RecurseError_forDir(result);
    curDir = result;
    if (curDir[curDir.size() - 1] != DIRSEP) curDir += DIRSEP;
    recurseStack.push(Level(dir, curDir.length()));
    continue;

  } // endwhile (true)

}

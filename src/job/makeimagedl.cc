/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download .jigdo/.template and file URLs

*/

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <config.h>

#include <fstream>

#include <compat.hh>
#include <jigdodownload.hh>
#include <log.hh>
#include <makeimagedl.hh>
#include <md5sum.hh>
#include <mimestream.hh>
#include <string.hh>

using namespace Job;
//______________________________________________________________________

MakeImageDl::MakeImageDl(IO* ioPtr, const string& jigdoUri,
                         const string& destination)
    : io(ioPtr), stateVal(DOWNLOADING_JIGDO),
      jigdoUrl(jigdoUri), jigdo(0), dest(destination), tmpDirVal(),
      mi(jigdoUri, this) {
  // Remove all trailing '/' from dest dir, even if result empty
  unsigned destLen = dest.length();
  while (destLen > 0 && dest[destLen - 1] == DIRSEP) --destLen;
  dest.resize(destLen);

  /* Create name of temporary dir.
     Directory name: "jigdo-" followed by md5sum of .jigdo URL */
  MD5Sum md;
  md.update(reinterpret_cast<const byte*>(dest.c_str()),
            dest.length()).finish();
  Base64String dirname;
  // Halve number of bits by XORing bytes. (Would a real CRC64 be better??)
  byte cksum[8];
  for (int i = 0; i < 8; ++i) cksum[i] = md.digest()[i] ^ md.digest()[i + 8];
  dirname.write(cksum, 8).flush();
  tmpDirVal = dest;
  tmpDirVal += DIRSEPS "jigdo-";
  tmpDirVal += dirname.result();
}

Job::MakeImageDl::~MakeImageDl() {
  delete jigdo;
}

void MakeImageDl::run() {
  Assert(!finalState());

  // Now create tmpdir
  int status = compat_mkdir(tmpDir().c_str());
  if (status != 0) {
    if (errno == EEXIST) {
      // FIXME: Resume if EEXIST
      msg("RESUME NOT YET SUPPORTED, SORRY");
    } else {
      string error = subst(_("Could not create temporary directory: "
                             "%L1"), strerror(errno));
      generateError(&error);
      return;
    }
  }
  writeReadMe();

#warning jigdo = dataSourceFor(jigdoUrl, ---);

  jigdo = new JigdoDownload(this, 0, jigdoUrl, mi.configFile().end());
  jigdo->run();
}

void MakeImageDl::error(const string& message) {
  throw Error(message);
//   string e(message);
//   job_failed(&e);
}
void MakeImageDl::info(const string&) {
  Assert(false); // ATM, is never called!
}
//______________________________________________________________________

void Job::MakeImageDl::writeReadMe() {
  string readmeName = tmpDir();
  readmeName += DIRSEP;
  readmeName += "ReadMe.txt";
  ofstream f(readmeName.c_str());
  f << subst(_(
    "Jigsaw Download - half-finished download\n"
    "\n"
    "This directory contains the data for a half-finished download of a\n"
    ".jigdo file. Do not change or delete any of the files in this\n"
    "directory! (Of course you can delete the entire directory if you do\n"
    "not want to continue with the download.)\n"
    "\n"
    "If the jigdo application was stopped and you want it to resume this\n"
    "download, simply enter again the same values you used the first time.\n"
    "\n"
    "In the \"URL\" field, enter:\n"
    "  %1\n"
    "\n"
    "In the \"Save to\" field, enter the parent directory of the directory\n"
    "containing this file. Unless you have moved it around, the correct\n"
    "value is:\n"
    "  %2\n"
    "\n"
    "\n"
    "[Download]\n"
    "URL=%1\n"
    "Destination=%2\n"
    ), jigdoUri(), dest);
}

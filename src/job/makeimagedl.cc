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

#include <compat.hh>
#include <jigdodownload.hh>
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
  dirname.write(md.digest(), 16).flush();
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
  //cerr<<"TMPDIR="<<tmpDirVal<<endl;
  int status = compat_mkdir(tmpDir().c_str());
  if (status != 0 /*&& errno != EEXIST*/) { // FIXME: Resume if EEXIST
    string error = subst(_("Could not create temporary directory: "
                           "%L1"), strerror(errno));
    generateError(&error);
    return;
  }

  jigdo = new JigdoDownload(this, 0, jigdoUrl, mi.configFile().end());
}

void MakeImageDl::error(const string& message) {
  throw Error(message);
//   string e(message);
//   job_failed(&e);
}
void MakeImageDl::info(const string&) {
  Assert(false); // ATM, is never called!
}

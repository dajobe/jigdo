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

#include <config.h>

#include <fstream>
#include <memory>

#include <compat.hh>
#include <jigdodownload.hh>
#include <log.hh>
#include <makeimagedl.hh>
#include <md5sum.hh>
#include <mimestream.hh>
#include <string.hh>

using namespace Job;
//______________________________________________________________________

DEBUG_UNIT("makeimagedl")

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
//______________________________________________________________________

Job::MakeImageDl::~MakeImageDl() {
  delete jigdo;
}
//______________________________________________________________________

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

  // Run initial .jigdo download, will start other downloads as needed
  auto_ptr<DataSource> dl(dataSourceFor(jigdoUrl, 0));
  auto_ptr<DataSource::IO> frontend(io->makeImageDl_new(dl.get(), "blubb"));
  jigdo = new JigdoIO(this, dl.get(), frontend.get());
  frontend.release();
  dl.release()->run();
}
//______________________________________________________________________

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
//______________________________________________________________________

DataSource* MakeImageDl::dataSourceFor(const string& url, const MD5* md) {
  debug("dataSourceFor: %1", url);

  string filename = tmpDir();
  filename += DIRSEP;
  Base64String b64;
  if (md == 0) {
    // Create leafname from url ("u" for "url")
    filename += "u-";
    MD5Sum nameMd;
    nameMd.update(reinterpret_cast<const byte*>(url.c_str()),
                  url.length()).finish();
    b64.write(nameMd.digest(), 16).flush();
  } else {
    // Create leafname from md ("c" for "content")
    filename += "c-";
    b64.write(*md, 16).flush();
  }
  filename += b64.result();

  // Check whether file already present in cache, i.e. in tmpDir
  struct stat fileInfo;
  int statResult = stat(filename.c_str(), &fileInfo);
  if (statResult == 0)
    return dataSourceForCompleted(fileInfo, filename, md);

  /* statResult != 0, we assume this means "no such file or directory".
     Now check whether a download is already under way, or if a half-finished
     download was aborted earlier. */
  Paranoid(filename[tmpDir().length() + 2] == '-');
  filename[tmpDir().length() + 2] = '~';
  statResult = stat(filename.c_str(), &fileInfo);
  if (statResult == 0)
    return dataSourceForSemiCompleted(fileInfo, filename);

  /* Neither the complete nor the partial data is in the cache, so start a
     new download. */
  debug("dataSourceFor: New download to %1", filename);
  return new SingleUrl(0, url);
}
//______________________________________________________________________

// Cache contains already completed download for requested URL/md5sum
DataSource* MakeImageDl::dataSourceForCompleted(
    const struct stat& fileInfo, const string& filename, const MD5* md) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%1' is not a file"),
                       filename);
    io->job_failed(&err);
    return 0;
  }
  if (md != 0) {
    // Data with that MD5 known - no need to go on the net, imm. return it
    debug("dataSourceFor: already have %1", filename);
    Assert(false); return 0;
  } else {
    /* Data for URL fetched before. If less than IF_MOD_SINCE seconds ago,
       just return it, else do an If-Modified-Since request. */
    debug("dataSourceFor: if-mod-since %1", filename);
    Assert(false); return 0;
  }
}
//______________________________________________________________________

DataSource* MakeImageDl::dataSourceForSemiCompleted(
    const struct stat& fileInfo, const string& filename) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%1' is not a file"),
                       filename);
    io->job_failed(&err);
    return 0;
  }
  debug("dataSourceFor: already have partial %1", filename);
  Assert(false); return 0;
#if 0
  if (anotherdownloadunderway) {
    return cloneofotherdownload; // What if other d/l is aborted by user
  } else {
    // Do a resume + If-Modified-Since
    return x;
  }
#endif
}

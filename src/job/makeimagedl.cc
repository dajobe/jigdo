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
#include <jigdo-io.hh>
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
  auto_ptr<Child> childDl(childFor(jigdoUrl, 0));
  if (childDl.get() != 0) {
    auto_ptr<DataSource::IO> frontend(
        io->makeImageDl_new(childDl->source(), tmpDir()) );
    childDl->setChildIo(new JigdoIO(childDl.get(), frontend.get()));
    frontend.release();
    string info = _("Retrieving .jigdo data");
    if (io) io->job_message(&info);
    jigdo = childDl.release();
    jigdo->source()->run();
  }
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

MakeImageDl::Child* MakeImageDl::childFor(const string& url, const MD5* md) {
  debug("dataSourceFor: %L1", url);

  bool contentMdKnown = (md != 0);
  MD5 cacheMd; // Will contain md5sum of either file URL or file contents
  string leafname;
  Base64String b64;
  if (contentMdKnown) {
    // Create leafname from md ("c" for "content")
    leafname += "c-";
    b64.write(*md, 16).flush();
    cacheMd = *md;
  } else {
    // Create leafname from url ("u" for "url")
    leafname += "u-";
    MD5Sum nameMd;
    nameMd.update(reinterpret_cast<const byte*>(url.c_str()),
                  url.length()).finish();
    b64.write(nameMd.digest(), 16).flush();
    cacheMd = nameMd;
  }
  leafname += b64.result();

  // Check whether file already present in cache, i.e. in tmpDir
  string filename = tmpDir();
  filename += DIRSEP;
  filename += leafname;
  struct stat fileInfo;
  int statResult = stat(filename.c_str(), &fileInfo);
  if (statResult == 0)
    return childForCompleted(fileInfo, filename, contentMdKnown, cacheMd);

  /* statResult != 0, we assume this means "no such file or directory".
     Now check whether a download is already under way, or if a half-finished
     download was aborted earlier. */
  Paranoid(filename[tmpDir().length() + 2] == '-');
  filename[tmpDir().length() + 2] = '~';
  Paranoid(leafname[1] == '-');
  leafname[1] = '~';
  statResult = stat(filename.c_str(), &fileInfo);
  if (statResult == 0)
    return childForSemiCompleted(fileInfo, filename);

  /* Neither the complete nor the partial data is in the cache, so start a
     new download. */
  debug("dataSourceFor: New download to %L1", filename);
  BfstreamCounted* f = new BfstreamCounted(filename.c_str(),
                                    ios::binary|ios::in|ios::out|ios::trunc);
  if (!*f) {
    string err = subst(_("Could not open `%L1' (%L2)"),
                       leafname, strerror(errno));
    generateError(&err);
    return 0;
  }
  auto_ptr<SingleUrl> dl(new SingleUrl(0, url));
  dl->setDestination(f, 0, 0);
  Child* c = new Child(this, dl.get(), contentMdKnown, cacheMd);
  dl.release();
  return c;
}
//______________________________________________________________________

// Cache contains already completed download for requested URL/md5sum
MakeImageDl::Child* MakeImageDl::childForCompleted(
    const struct stat& fileInfo, const string& filename, bool contentMdKnown,
    const MD5& /*cacheMd*/) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%L1' is not a file"),
                       filename);
    io->job_failed(&err);
    return 0;
  }
  if (contentMdKnown) {
    // Data with that MD5 known - no need to go on the net, imm. return it
    debug("dataSourceFor: already have %L1", filename);
    Assert(false); return 0;
  } else {
    /* Data for URL fetched before. If less than IF_MOD_SINCE seconds ago,
       just return it, else do an If-Modified-Since request. */
    debug("dataSourceFor: if-mod-since %L1", filename);
    Assert(false); return 0;
  }
}
//______________________________________________________________________

MakeImageDl::Child* MakeImageDl::childForSemiCompleted(
    const struct stat& fileInfo, const string& filename) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%L1' is not a file"),
                       filename);
    io->job_failed(&err);
    return 0;
  }
  debug("dataSourceFor: already have partial %L1", filename);
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
//______________________________________________________________________

void MakeImageDl::childSucceeded(
    Child* childDl, DataSource::IO* /*childIo*/, DataSource::IO* frontend) {
  if (frontend != 0)
    io->makeImageDl_finished(childDl->source(), frontend);
  #warning // Rename u~... to u-...
  // TODO Delete child (sometimes)
}

void MakeImageDl::childFailed(
    Child* childDl, DataSource::IO* /*childIo*/, DataSource::IO* frontend) {
  if (frontend != 0)
    io->makeImageDl_finished(childDl->source(), frontend);
}

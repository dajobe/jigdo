/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  IO object for .jigdo downloads; download, gunzip, interpret

*/

#include <config.h>

#include <debug.hh>
#include <jigdo-io.hh>
#include <log.hh>
#include <makeimagedl.hh>
#include <md5sum.hh>
//______________________________________________________________________

DEBUG_UNIT("jigdo-io")

using namespace Job;

JigdoIO::JigdoIO(MakeImageDl::Child* c, DataSource::IO* frontendIo)
    : childDl(c), frontend(frontendIo), parent(0),
      includeLine(0), firstChild(0), next(0),
      rootAndImageSectionCandidate(0), gunzip(this), line(0),
      imageSectionLine(0), imageName(), imageInfo(), imageShortInfo(),
      templateMd5(0) { }
//______________________________________________________________________

JigdoIO::~JigdoIO() {
  debug("~JigdoIO");
  if (source() != 0) master()->childFailed(childDl, this, frontend);
  // childDl->deleteSource(); will be called by the Child which owns us
}
//______________________________________________________________________

Job::IO* JigdoIO::job_removeIo(Job::IO* rmIo) {
  debug("job_removeIo %1", rmIo);
  if (rmIo == this) {
    // Should never be called for jigdo
    Assert(false);
    master()->childFailed(childDl, this, frontend);
    Job::IO* c = frontend;
    // Do not "delete this" - childDl owns us and the SingleUrl
    return c;
  } else if (frontend != 0) {
    Job::IO* c = frontend->job_removeIo(rmIo);
    Paranoid(c == 0 || dynamic_cast<DataSource::IO*>(c) != 0);
    frontend = static_cast<DataSource::IO*>(c);
  }
  return this;
}

void JigdoIO::job_deleted() {
  if (frontend != 0) frontend->job_deleted();
  // Do not "delete this" - childDl owns us and the SingleUrl
}

void JigdoIO::job_succeeded() {
  if (frontend != 0) {
    frontend->job_succeeded();
    master()->childSucceeded(childDl, this, frontend);
    childDl->deleteSource();
  }
}

void JigdoIO::job_failed(string* message) {
  if (frontend != 0) {
    frontend->job_failed(message);
    string jigdoFailed = _("Download of .jigdo file failed");
    master()->generateError(&jigdoFailed);
    /* It might make sense to call this here, but we don't because if we did,
       GtkSingleUrl would auto-remove the info about the child (including
       error message) after a few seconds. It will be called from the dtor
       instead.
       master()->childFailed(childDl, this, frontend);
       childDl->deleteSource(); */
  }
}

void JigdoIO::job_message(string* message) {
  if (frontend != 0) frontend->job_message(message);
}

void JigdoIO::dataSource_dataSize(uint64 n) {
  if (frontend != 0) frontend->dataSource_dataSize(n);
}

void JigdoIO::dataSource_data(const byte* data, size_t size,
                              uint64 currentSize) {
  if (master()->finalState()) return;
  Assert(master()->state() == MakeImageDl::DOWNLOADING_JIGDO);
  debug("Got %1 bytes", size);
  try {
    gunzip.inject(data, size);
  } catch (Error e) {
    string err = subst("%L1:%2: %3", source()->location(), line, e.message);
    job_failed(&err);
    string jigdoFailed = _("Unzipping of .jigdo file failed");
    master()->generateError(&jigdoFailed);
    return;
  }
  if (frontend != 0) frontend->dataSource_data(data, size, currentSize);
}
//______________________________________________________________________

void JigdoIO::gunzip_deleted() { }

void JigdoIO::gunzip_needOut(Gunzip*) {
  /* This is only called once, at the very start - afterwards, we always call
     setOut() from gunzip_data, so Gunzip won't call this. */
  gunzip.setOut(gunzipBuf, GUNZIP_BUF_SIZE);
}

/* Uncompressed data arrives. "decompressed" points somewhere inside
   gunzipBuf. Split data apart at \n and interpret line(s), then copy any
   remaining unfinished line to the start of gunzipBuf. The first byte of
   gunzipBuf (if it contains valid data) is always the first char of a line
   in the config file. */
void JigdoIO::gunzip_data(Gunzip*, byte* decompressed, unsigned size) {
//   // If an error happened earlier, ignore this call to gunzip_data()
//   if (gunzipBuf == 0) return;

  // Look for end of line.
  byte* p = decompressed;
  const byte* end = decompressed + size;
  const byte* stringStart = gunzipBuf;
  string line;

  while (p < end) {
    if (*p == '\n') {
      // Add new line to ConfigFile
      Paranoid(static_cast<unsigned>(p - stringStart) <= GUNZIP_BUF_SIZE);
      Paranoid(line.empty());
      const char* lineChars = reinterpret_cast<const char*>(stringStart);
      if (g_utf8_validate(lineChars, p - stringStart, NULL) != TRUE)
        throw Error(_("Input .jigdo data is not valid UTF-8"));
      line.append(lineChars, p - stringStart);
      debug("jigdo line: `%1'", line);
//       configFile().push_back();
//       swap(configFile().back(), line);
line.erase();
      ++p;
      stringStart = p;
      continue;
    }
    if (*p == '\r')
      *p = ' '; // Allow Windows-style line endings by turning CR into space
    else if (*p == 127 || (*p < 32 && *p != '\t')) // Check for evil chars
     throw Error(_("Input .jigdo data contains invalid control characters"));
    ++p;
  }

  if (stringStart == gunzipBuf && p == stringStart + GUNZIP_BUF_SIZE) {
    // A single line fills the whole buffer. Truncate it at that length.
    debug("gunzip_data: long line");
    Paranoid(line.empty());
    const char* lineChars = reinterpret_cast<const char*>(stringStart);
    if (g_utf8_validate(lineChars, p - stringStart, NULL) != TRUE)
      throw Error(_("Input .jigdo data is not valid UTF-8"));
    line.append(lineChars, p - stringStart);
    debug("jigdo line: \"%1\"", line);
//     configFile().push_back();
//     swap(configFile().back(), line);
line.erase();
    // Trick: To ignore remainder of huge line, prepend a comment char '#'
    gunzipBuf[0] = '#';
    gunzip.setOut(gunzipBuf + 1, GUNZIP_BUF_SIZE - 1);
    return;
  }

  unsigned len = p - stringStart;
  if (len > 0 && stringStart > gunzipBuf) {
    // Unprocessed data left somewhere inside the buffer - copy to buf start
    Assert(len < GUNZIP_BUF_SIZE); // Room must be left in the buffer
    memmove(gunzipBuf, stringStart, len);
  }
  gunzip.setOut(gunzipBuf + len, GUNZIP_BUF_SIZE - len);
}

void JigdoIO::gunzip_failed(string* message) {
  throw Error(*message, true);
}

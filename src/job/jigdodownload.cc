/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  JigdoDownload is a private nested class of MakeImageDl.

  Download .jigdo file. If the data that is fetched from the primary .jigdo
  URL contains [Include] directives, additional JigdoDownloads will be
  started for each one.

  To deal with [Include], the following happens: Each JigdoDownload has an
  iterator at which it inserts lines of downloaded data into the MakeImage's
  JigdoConfig. When an [Include] is encountered and the maximum allowed level
  of includes is not yet reached, a new JigdoDownload is set up to insert
  lines just *before* the [Include] line, and only to remove that [Include]
  line when the download has successfully completed.

  When asked for e.g. the filename of the output file, MakeImage needs to
  access the *first* [Image] section in the .jigdo data, regardless of
  whether that appears in the top-level .jigdo file or a file included from
  there. It must ignore any further [Image] sections. So while searching
  through the data, it'll stop at any [Include] and say "[Image]" not yet
  found.

*/

#include <config.h>

#include <iostream>

#include <jigdodownload.hh>
#include <log.hh>

using namespace Job;
//______________________________________________________________________

DEBUG_UNIT("jigdodownload")

MakeImageDl::JigdoDownload::JigdoDownload(MakeImageDl* m, JigdoDownload* p,
                                          const string& jigdoUrl,
                                          ConfigFile::iterator destPos)
    : SingleUrl(this, jigdoUrl), master(m), parent(p), ioVal(0),
      gunzipBuf(), gunzip(this), insertPos(destPos) {
  ioVal.set(master->io->makeImageDl_new(this));
}

MakeImageDl::JigdoDownload::~JigdoDownload() {
  master->io->makeImageDl_finished(this);
  if (master->jigdo == this) master->jigdo = 0;
  SingleUrl::io().set(0);
}

void MakeImageDl::JigdoDownload::run() {
  SingleUrl::run(0, 0, 0, 0, false);
}
//______________________________________________________________________

IOPtr<DataSource::IO>& MakeImageDl::JigdoDownload::io() {
  return ioVal;
}

const IOPtr<DataSource::IO>& MakeImageDl::JigdoDownload::io() const {
  return ioVal;
}
//______________________________________________________________________

void MakeImageDl::JigdoDownload::job_deleted() {
  if (ioVal) ioVal->job_deleted();
}

void MakeImageDl::JigdoDownload::job_succeeded() {
  if (ioVal) ioVal->job_succeeded();
  Assert(master->state() == DOWNLOADING_JIGDO);
  master->stateVal = DOWNLOADING_TEMPLATE;
  delete this;
}
void MakeImageDl::JigdoDownload::job_failed(string* message) {
  if (ioVal) ioVal->job_failed(message);
}
void MakeImageDl::JigdoDownload::job_message(string* message) {
  if (ioVal) ioVal->job_message(message);
}
void MakeImageDl::JigdoDownload::dataSource_dataSize(uint64 n) {
  if (ioVal) ioVal->dataSource_dataSize(n);
}
void MakeImageDl::JigdoDownload::dataSource_data(const byte* data,
                                                size_t size,
                                                uint64 currentSize) {
  if (master->state() == ERROR) return;
  Assert(master->state() == DOWNLOADING_JIGDO);
  debug("Got %1 bytes", size);
  try {
    gunzip.inject(data, size);
  } catch (Error e) {
    master->io->job_failed(&e.message);
    master->stateVal = ERROR;
    return;
  }
  if (ioVal) ioVal->dataSource_data(data, size, currentSize);
}
//______________________________________________________________________

void MakeImageDl::JigdoDownload::gunzip_deleted() { }

void MakeImageDl::JigdoDownload::gunzip_needOut(Gunzip*) {
  /* This is only called once, at the very start - afterwards, we always call
     setOut() from gunzip_data, so Gunzip won't call this. */
  gunzip.setOut(gunzipBuf, GUNZIP_BUF_SIZE);
}

/* Uncompressed data arrives. "decompressed" points somewhere inside
   gunzipBuf. Split data apart at \n and add lines to the ConfigFile, then
   copy any remaining unfinished line to the start of gunzipBuf. The first
   byte of gunzipBuf (if it contains valid data) is always the first char of
   a line in the config file. */
void MakeImageDl::JigdoDownload::gunzip_data(Gunzip*, byte* decompressed, unsigned size) {
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
      configFile().push_back();
      swap(configFile().back(), line);
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
    configFile().push_back();
    swap(configFile().back(), line);
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

void MakeImageDl::JigdoDownload::gunzip_failed(string* message) {
  throw Error(*message, true);
}

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2002-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Single HTTP or FTP retrievals

*/

#include <config.h>

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <iostream>
#include <fstream>

#include <autoptr.hh>
#include <debug.hh>
#include <log.hh>
#include <single-url.hh>
#include <string-utf.hh>

using namespace Job;
//______________________________________________________________________

DEBUG_UNIT("single-url")

SingleUrl::SingleUrl(DataSource::IO* ioPtr, const string& uri)
  : DataSource(ioPtr), download(uri, this), progressVal(),
    stopLaterId(0), destStreamVal(0), destOff(0), destEndOff(0),
    resumeLeft(0), haveResumeOffset(false), haveDestination(false),
    havePragmaNoCache(false), tries(0) {
}
//________________________________________

SingleUrl::~SingleUrl() {
  debug("~SingleUrl %1", this);
  if (!download.failed() && !download.succeeded())
    download.stop();
  if (stopLaterId != 0) {
    // The chance that this code is ever reached is microscopic, but still...
    g_source_remove(stopLaterId);
  }
}

const Progress* SingleUrl::progress() const { return &progressVal; }

const string& SingleUrl::location() const { return download.uri(); }
//______________________________________________________________________

void SingleUrl::setResumeOffset(uint64 resumeOffset) {
  if (resumeOffset < RESUME_SIZE)
    resumeLeft = resumeOffset;
  else
    resumeLeft = RESUME_SIZE;
  // Not "resumeOffset - resumeLeft", won't be calling io for a while:
  progressVal.setCurrentSize(resumeOffset);
  download.setResumeOffset(resumeOffset - resumeLeft);

  haveResumeOffset = true;
}

void SingleUrl::setDestination(BfstreamCounted* destStream,
                               uint64 destOffset, uint64 destEndOffset) {
  destStreamVal = destStream;
  destOff = destOffset;
  destEndOff = destEndOffset;

  haveDestination = true;
}

void SingleUrl::run() {
  if (stopLaterId != 0) {
    g_source_remove(stopLaterId);
    stopLaterId = 0;
  }

  if (!haveResumeOffset) setResumeOffset(0);
  haveResumeOffset = false;
  if (!haveDestination) setDestination(0, 0, 0);
  haveDestination = false;
  if (!havePragmaNoCache) setPragmaNoCache(false);
  havePragmaNoCache = false;

  if (destEndOff > destOff)
    progressVal.setDataSize(destEndOff - destOff);
  else
    progressVal.setDataSize(0);

  ++tries;

  Assert(resumeLeft == 0 || destStreamVal != 0);

  progressVal.reset();
  download.run();
}
//______________________________________________________________________

void SingleUrl::resumeFailed() {
  setNoResumePossible();
  string error(_("Resume failed"));
  if (io) io->job_failed(&error);
  stopLater();
  return;
}

void SingleUrl::stopLater() {
  if (stopLaterId != 0) return;
  stopLaterId = g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                                &stopLater_callback,
                                (gpointer)this, NULL);
  Assert(stopLaterId != 0); // because we use 0 as a special value
}

gboolean SingleUrl::stopLater_callback(gpointer data) {
  debug("stopLater_callback");
  SingleUrl* self = static_cast<SingleUrl*>(data);
  self->download.stop();
  self->stopLaterId = 0;
  return FALSE; // "Don't call me again"
}
//______________________________________________________________________

void SingleUrl::download_dataSize(uint64 n) {
  if (progressVal.dataSize() == 0) {
    progressVal.setDataSize(n);
  } else {
    // Error if reported size of object does not match expected one
    if (n > 0 && n != progressVal.dataSize()) resumeFailed();
  }
  if (!resuming()) {
    if (io) io->dataSource_dataSize(n);
    return;
  }
}
//______________________________________________________________________

bool SingleUrl::writeToDestStream(uint64 off, const byte* data,
                                  unsigned size) {
  if (destStream() == 0 || stopLaterId != 0) return SUCCESS;
  debug("writeToDestStream %1 %2 bytes at offset %3",
        destStream(), size, off);

  // Never go beyond destEndOff
  unsigned realSize = size;
  if (destOff < destEndOff
      && off + size > destEndOff)
    realSize = destEndOff - off;

  destStream()->seekp(off, ios::beg);
  writeBytes(*destStream(), data, realSize);
  if (!*destStream()) {
    stopLater();
    if (io) {
      string error = subst("%L1", strerror(errno));
      io->job_failed(&error);
    }
    return FAILURE;
  }
  if (realSize < size) {
    // Server sent more than we expected; error
    stopLater();
    if (io) {
      string error = _("Server sent more data than expected");
      io->job_failed(&error);
    }
    return FAILURE;
  }
  return SUCCESS;
}
//______________________________________________________________________

void SingleUrl::download_data(const byte* data, unsigned size,
                              uint64 currentSize) {
# if DEBUG
  Paranoid(resuming() || progressVal.currentSize() == currentSize - size);
  //g_usleep(10000);
  string s;
  for (unsigned i = 0; i < (size < 65 ? size : 65); ++i)
    s += ((data[i] >= ' '&&data[i] < 127 || data[i] >= 160) ? data[i] : '.');
  debug("Got %1 bytes [%2,%3]: %4",
        size, progressVal.currentSize(), currentSize - size, s);
# endif

  if (!progressVal.autoTick() // <-- extra check for efficiency only
      && !download.pausedSoon())
    progressVal.setAutoTick(true);

  if (!resuming()) {
    // Normal case: Just write it to file and forward it downstream
    progressVal.setCurrentSize(currentSize);
    if (writeToDestStream(destOff + currentSize - size, data, size)
        == FAILURE) return;
    if (io) io->dataSource_data(data, size, currentSize);
    return;
  }
  //____________________

  /* We're in the middle of a resume - compare downloaded bytes with bytes
     from destStream, don't pass them on. Note that progressVal.currentSize()
     is "stuck" at the value currentSize+resumeLeft, i.e. the offset of the
     end of the overlap area. */
  debug("RESUME left=%1 off=%2 currentSize=%3",
        resumeLeft, destOff + currentSize - size, currentSize);

  // If resume already failed, ignore further calls
  if (stopLaterId != 0) return;

  // Read from file
  unsigned toRead = min(resumeLeft, size);
  byte buf[toRead];
  byte* bufEnd = buf + toRead;
  byte* b = buf;
  destStream()->seekg(destOff + currentSize - size, ios::beg);
  while (*destStream() && toRead > 0) {
    readBytes(*destStream(), b, toRead);
    size_t n = destStream()->gcount();
    b += n;
    toRead -= n;
  }
  if (toRead > 0 && !*destStream()) { resumeFailed(); return; }

  // Compare
  b = buf;
  while (size > 0 && b < bufEnd) {
    if (*data != *b) {
      resumeFailed();
      debug("  fromfile=%1 fromnet=%2", int(*b), int(*data));
      return;
    }
    ++data; ++b; --size; --resumeLeft;
  }

  string info = subst(_("Resuming... %1kB"), resumeLeft / 1024);
  if (io) io->job_message(&info);

  if (resumeLeft > 0) return;

  /* Success: End of buf reached and all bytes matched. Pass remaining bytes
     from this chunk to IO. */
  progressVal.reset();
  if (size > 0) {
    progressVal.setCurrentSize(currentSize);
    if (writeToDestStream(destOff + currentSize - size, data, size)
        == FAILURE) return;
    if (io) io->dataSource_data(data, size, currentSize);
  }
}
//______________________________________________________________________

void SingleUrl::download_succeeded() {
  // Was download aborted prematurely? If yes, resume.
  progressVal.setAutoTick(false);
//   uint64 currentSize = progressVal.currentSize();
//   uint64 dataSize = progressVal.dataSize();
//   if (dataSize > 0 && currentSize < dataSize) {
//     string error = "Data transfer interrupted";
//     if (resuming()) {
//       /* Fix progress.currentSize() so resume won't subtract yet another
//          RESUME_SIZE bytes later */
//       progressVal.setCurrentSize(currentSize + (resumeEnd - resumePos));
//     }
//     io->job_failed(&error);
//     return;
//   }
  if (stopLaterId != 0) return;
  if (io) io->job_succeeded();
}
//______________________________________________________________________

void SingleUrl::download_failed(string* message) {
  progressVal.setAutoTick(false);
  if (resuming()) {
    /* Fix progress.currentSize() so resume won't subtract yet another
       RESUME_SIZE bytes later */
    progressVal.setCurrentSize(progressVal.currentSize() + resumeLeft);
  }
  if (io) io->job_failed(message);
}
//______________________________________________________________________

void SingleUrl::download_message(string* message) {
  if (io) io->job_message(message);
}
//______________________________________________________________________

bool SingleUrl::paused() const {
  return download.pausedSoon();
}

void SingleUrl::pause() {
  Paranoid(!paused());
  download.pause();
  progressVal.setAutoTick(false);
}

void SingleUrl::cont() {
  Paranoid(paused());
  progressVal.reset();
  download.cont();
  // progressVal.setAutoTick(true) called from download_data() later
}
//______________________________________________________________________


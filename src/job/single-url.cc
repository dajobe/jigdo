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

SingleUrl::SingleUrl(/*IOPtr DataSource::IO* ioPtr, */const string& uri)
  : DataSource(/*ioPtr*/), download(uri, this), progressVal(),
    destStreamVal(0), destOff(0), destEndOff(0), resumeLeft(0),
    haveResumeOffset(false), haveDestination(false),
    /*havePragmaNoCache(false),*/ tries(0) {
  debug("SingleUrl %1", this);
}
//________________________________________

SingleUrl::~SingleUrl() {
  debug("~SingleUrl %1", this);
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
  debug("SingleUrl %1 run()", this);
  if (!haveResumeOffset) setResumeOffset(0);
  haveResumeOffset = false;
  if (!haveDestination) setDestination(0, 0, 0);
  haveDestination = false;
//   if (!havePragmaNoCache) setPragmaNoCache(false);
//   havePragmaNoCache = false;

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
  debug("resumeFailed");
  setNoResumePossible();
  string error(_("Resume failed"));
  IOSOURCE_SEND(DataSource::IO, io, job_failed, (error));
  download.stop();
  progressVal.setAutoTick(false);
  return;
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
    IOSOURCE_SEND(DataSource::IO, io, dataSource_dataSize, (n));
    return;
  }
}
//______________________________________________________________________

bool SingleUrl::writeToDestStream(uint64 off, const byte* data,
                                  unsigned size) {
  if (destStream() == 0 /*|| stopLaterId != 0*/) return SUCCESS;
  //debug("writeToDestStream %1 %2 bytes at offset %3",
  //      destStream(), size, off);

  // Never go beyond destEndOff
  unsigned realSize = size;
  if (destOff < destEndOff
      && off + size > destEndOff)
    realSize = destEndOff - off;

  destStream()->seekp(off, ios::beg);
  writeBytes(*destStream(), data, realSize);
  if (!*destStream()) {
    string error = subst("%L1", strerror(errno));
    IOSOURCE_SEND(DataSource::IO, io, job_failed, (error));
    download.stop();
    //stopLater();
    progressVal.setAutoTick(false);
    return FAILURE;
  }
  if (realSize < size) {
    // Server sent more than we expected; error
    string error = _("Server sent more data than expected");
    IOSOURCE_SEND(DataSource::IO, io, job_failed, (error));
    download.stop();
    //stopLater();
    progressVal.setAutoTick(false);
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
  unsigned limit = (size < 60 ? size : 60);
  for (unsigned i = 0; i < limit; ++i)
    if (data[i] >= 32 && data[i] < 127) s += data[i]; else s += '.';
  debug("%5 Got %1 currentSize=%2, realoffset=%3: %4",
        size, progressVal.currentSize(), currentSize - size, s, this);
# endif

  // && !download.pausedSoon())
  if (!progressVal.autoTick() // <-- extra check for efficiency only
      && !download.paused())
    progressVal.setAutoTick(true);

  if (!resuming()) {
    // Normal case: Just write it to file and forward it downstream
    progressVal.setCurrentSize(currentSize);
    if (writeToDestStream(destOff + currentSize - size, data, size)
        == FAILURE) return;
    IOSOURCE_SEND(DataSource::IO, io,
                  dataSource_data, (data, size, currentSize));
    return;
  }
  //____________________

  /* We're in the middle of a resume - compare downloaded bytes with bytes
     from destStream, don't pass them on. Note that progressVal.currentSize()
     is "stuck" at the value currentSize+resumeLeft, i.e. the offset of the
     end of the overlap area. */
  debug("RESUME left=%1 off=%2 fileoff=%3", resumeLeft,
        destOff + currentSize - size, destOff + currentSize - size);

  // Read from file
  unsigned toRead = min(resumeLeft, size);
  byte buf[toRead];
  byte* bufEnd = buf + toRead;
  byte* b = buf;
  destStream()->seekg(destOff + currentSize - size, ios::beg);
  while (*destStream() && toRead > 0) {
    readBytes(*destStream(), b, toRead);
    size_t n = destStream()->gcount();
    //debug("during resume: read %1", n);
    b += n;
    toRead -= n;
  }
  if (toRead > 0 && !*destStream()) {
    debug("  Error toRead=%1 `%L2'", toRead, strerror(errno));
    resumeFailed(); return;
  }

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
  IOSOURCE_SEND(DataSource::IO, io, job_message, (info));

  if (resumeLeft > 0) return;

  /* Success: End of buf reached and all bytes matched. Pass remaining bytes
     from this chunk to IO. */
  progressVal.reset();
  if (size > 0) {
    debug("  End, currentSize now %1", currentSize);
    progressVal.setCurrentSize(currentSize);
    if (writeToDestStream(destOff + currentSize - size, data, size)
        == FAILURE) return;
    IOSOURCE_SEND(DataSource::IO, io,
                  dataSource_data, (data, size, currentSize));
  }
}
//______________________________________________________________________

void SingleUrl::download_succeeded() {
  progressVal.setAutoTick(false);
  IOSOURCE_SEND(DataSource::IO, io, job_succeeded, ());
}
//______________________________________________________________________

void SingleUrl::download_failed(string* message) {
  progressVal.setAutoTick(false);
  IOSOURCE_SEND(DataSource::IO, io, job_failed, (*message));
}
//______________________________________________________________________

void SingleUrl::download_message(string* message) {
  IOSOURCE_SEND(DataSource::IO, io, job_message, (*message));
}
//______________________________________________________________________

bool SingleUrl::paused() const {
  //return download.pausedSoon();
  return download.paused();
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


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
  : ioVal(ioPtr), download(uri, this), progressVal(), resumeFailedId(0),
    destStreamVal(0), destOff(0), destEndOff(0), resumeLeft(0), tries(0) {
}

void SingleUrl::run(uint64 resumeOffset, bfstream* destStream,
                    uint64 destOffset, uint64 destEndOffset,
                    bool pragmaNoCache) {
  if (resumeFailedId != 0) {
    g_source_remove(resumeFailedId);
    resumeFailedId = 0;
  }

  destStreamVal = destStream;
  destOff = destOffset;
  destEndOff = destEndOffset;
  if (destEndOffset > destOffset)
    progressVal.setDataSize(destEndOffset - destOffset);
  ++tries;

  Assert(resumeOffset == 0 || destStreamVal != 0);
  if (resumeOffset < RESUME_SIZE)
    resumeLeft = resumeOffset;
  else
    resumeLeft = RESUME_SIZE;
  // Not "resumeOffset - resumeLeft", won't be calling io for a while:
  progressVal.setCurrentSize(resumeOffset);
  progressVal.reset();
  download.run(resumeOffset - resumeLeft, pragmaNoCache);
}
//________________________________________

SingleUrl::~SingleUrl() {
  if (!download.failed() && !download.succeeded())
    download.stop();
  if (resumeFailedId != 0) {
    // The chance that this code is ever reached is microscopic, but still...
    g_source_remove(resumeFailedId);
  }
}
//______________________________________________________________________

IOPtr<DataSource::IO>& SingleUrl::io() {
  return ioVal;
}

const IOPtr<DataSource::IO>& SingleUrl::io() const {
  return ioVal;
}
//______________________________________________________________________

void SingleUrl::resumeFailed() {
  if (resumeFailedId != 0) return;
  resumeFailedId = g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                                   &resumeFailed_callback,
                                   (gpointer)this, NULL);
  Assert(resumeFailedId != 0); // because we use 0 as a special value
  return;
}

gboolean SingleUrl::resumeFailed_callback(gpointer data) {
  debug("resumeFailed_callback");
  SingleUrl* self = static_cast<SingleUrl*>(data);
  self->download.stop();
  self->setNoResumePossible();
  string error(_("Resume failed"));
  if (self->ioVal) self->ioVal->job_failed(&error);

  self->resumeFailedId = 0;
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
    if (ioVal) ioVal->dataSource_dataSize(n);
    return;
  }
}
//______________________________________________________________________

bool SingleUrl::writeToDestStream(uint64 off, const byte* data,
                                  unsigned size) {
  if (destStream() == 0) return SUCCESS;

  // Never go beyond destEndOff
  unsigned realSize = size;
  if (destOff < destEndOff
      && off + size > destEndOff)
    realSize = destEndOff - off;

  destStream()->seekp(off, ios::beg);
  writeBytes(*destStream(), data, realSize);
  if (!destStream()->good()) {
    download.stop();
    if (ioVal) {
      string error = subst("%L1", strerror(errno));
      ioVal->job_failed(&error);
    }
    return FAILURE;
  }
  if (realSize < size) {
    // Server sent more than we expected; error
    download.stop();
    if (ioVal) {
      string error = _("Server sent more data than expected");
      ioVal->job_failed(&error);
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
    if (ioVal) ioVal->dataSource_data(data, size, currentSize);
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
  if (resumeFailedId != 0) return;

  // Read from file
  unsigned toRead = min(resumeLeft, size);
  byte buf[toRead];
  byte* bufEnd = buf + toRead;
  byte* b = buf;
  destStream()->seekg(destOff + currentSize - size, ios::beg);
  while (destStream()->good() && toRead > 0) {
    readBytes(*destStream(), b, toRead);
    size_t n = destStream()->gcount();
    b += n;
    toRead -= n;
  }
  if (toRead > 0 && !destStream()->good()) { resumeFailed(); return; }

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
  if (ioVal) ioVal->job_message(&info);

  if (resumeLeft > 0) return;

  /* Success: End of buf reached and all bytes matched. Pass remaining bytes
     from this chunk to IO. */
  progressVal.reset();
  if (size > 0) {
    progressVal.setCurrentSize(currentSize);
    if (writeToDestStream(destOff + currentSize - size, data, size)
        == FAILURE) return;
    if (ioVal) ioVal->dataSource_data(data, size, currentSize);
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
//     ioVal->job_failed(&error);
//     return;
//   }
  if (ioVal) ioVal->job_succeeded();
}
//______________________________________________________________________

void SingleUrl::download_failed(string* message) {
  progressVal.setAutoTick(false);
  if (resuming()) {
    /* Fix progress.currentSize() so resume won't subtract yet another
       RESUME_SIZE bytes later */
    progressVal.setCurrentSize(progressVal.currentSize() + resumeLeft);
  }
  if (ioVal) ioVal->job_failed(message);
}
//______________________________________________________________________

void SingleUrl::download_message(string* message) {
  if (ioVal) ioVal->job_message(message);
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


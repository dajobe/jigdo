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

#include <glib.h>
#include <iostream>

#include <autoptr.hh>
#include <debug.hh>
#include <single-url.hh>
#include <string-utf.hh>

using namespace Job;
//______________________________________________________________________

SingleUrl::SingleUrl(IO* ioPtr, const string& uri, uint64 offset,
                     const byte* buf, size_t bufLen, bool pragmaNoCache)
  : ioVal(ioPtr), download(uri, this), progressVal(), resumeFailedId(0),
    resumeStart(buf), resumePos(buf), resumeEnd(buf + bufLen), tries(1) {
  if (offset == 0) {
    download.run(pragmaNoCache);
  } else {
    /* NB it is possible that bufLen == 0 here; in that case, buf may or may
       not be null as well. */
    if (bufLen == 0) {
      delete[] resumeStart;
      resumeStart = resumePos = resumeEnd = 0;
    }
    progressVal.setCurrentSize(offset - bufLen);
    progressVal.reset();
    download.resume(offset - bufLen);
  }
}

void SingleUrl::resume(const byte* buf, size_t bufLen) {
  Assert(resumePossible());
  Assert(buf != 0 || bufLen == 0);
  ++tries;
  uint64 offset = progressVal.currentSize();

  // Compare with ctor above
  Paranoid(resumeFailedId == 0);
  resumeStart = buf;
  resumePos = buf;
  resumeEnd = buf + bufLen;
  if (offset == 0) {
    progressVal.reset();
    download.run();
  } else {
    if (bufLen == 0) {
      delete[] resumeStart;
      resumeStart = resumePos = resumeEnd = 0;
    }
    progressVal.setCurrentSize(offset - bufLen);
    progressVal.reset();
    download.resume(offset - bufLen);
  }
}
//________________________________________

SingleUrl::~SingleUrl() {
  if (!download.failed() && !download.succeeded())
    download.stop();
  delete[] resumeStart;
  if (resumeFailedId != 0) {
    // The chance that this code is ever reached is microscopic, but still...
    g_source_remove(resumeFailedId);
  }
}
//______________________________________________________________________

IOPtr<SingleUrl::IO>& SingleUrl::io() {
  return ioVal;
}

const IOPtr<SingleUrl::IO>& SingleUrl::io() const {
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
# if DEBUG
  cerr << "SingleUrl::resumeFailed_callback" << endl;
# endif
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
  progressVal.setDataSize(n);
  if (!resuming()) {
    if (ioVal) ioVal->singleURL_dataSize(n);
    return;
  }

  /* Create error in the case that the resume buffer contains more bytes than
     the server claims to be able to give us; IOW, the file has changed on
     the server (it was truncated). */
  if (n > 0 && progressVal.currentSize() + (resumeEnd - resumePos) > n)
    resumeFailed();
}
//______________________________________________________________________

void SingleUrl::download_data(const byte* data, size_t size,
                              uint64 currentSize) {
# if DEBUG
  //g_usleep(10000);
# endif
  /*
    cerr << "Got " << size << " bytes ["<<progressVal.currentSize()<<"]: ";
    if (size > 80) size = 80;
    while (size > 0 && (*data >= ' ' && *data < 127 || *data >= 160))
      cerr << *data++;
    cerr << endl;
  */
  if (!progressVal.autoTick() // <-- extra check for efficiency only
      && !download.pausedSoon())
    progressVal.setAutoTick(true);

  progressVal.setCurrentSize(currentSize);

  if (!resuming()) {
    // Normal case: Just forward it all downstream
    if (ioVal) ioVal->singleURL_data(data, size, currentSize);
    return;
  }

  /* We're in the middle of a resume - compare downloaded bytes with bytes in
     buffer, don't pass them on */

  // If resume already failed, ignore further calls
  if (resumeFailedId != 0) return;

  // Got new data, compare it to the data in buf
  while (size > 0 && resumePos < resumeEnd) {
    //cerr<<int(*data)<<' '<<int(*resumePos)<<endl;
    if (*data != *resumePos) {
      resumeFailed();
      return;
    }
    ++data; ++resumePos; --size;
  }

  string info = subst(_("Resuming... %1kB"), (resumeEnd - resumePos) / 1024);
  if (ioVal) ioVal->job_message(&info);

  if (resumePos < resumeEnd) return;

  /* Success: End of buf reached and all bytes matched. Pass remaining bytes
     from this chunk to IO. */
  Assert(currentSize - size
         == download.resumeOffset() + (resumeEnd - resumeStart));
  if (size > 0 && ioVal) ioVal->singleURL_data(data, size, currentSize);
  delete[] resumeStart;
  resumeStart = resumePos = resumeEnd = 0;
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
    progressVal.setCurrentSize(progressVal.currentSize()
                               + (resumeEnd - resumePos));
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


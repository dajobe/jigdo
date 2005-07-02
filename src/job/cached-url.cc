/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

  Spool data from cache file

  LIGHTLY TESTED ONLY, BOUND TO CONTAIN BUGS

*/

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd-jigdo.h>
#include <string.h>
#include <errno.h>
#include <fstream>

#include <autoptr.hh>
#include <cached-url.hh>
#include <log.hh>
//______________________________________________________________________

DEBUG_UNIT("cached-url")

using namespace Job;

namespace {

  /* Maximum nr of microseconds the idle callback funtion spoolDataCallback()
     should run before giving up control again. This is intentionally a
     multiple of the value of the GTK+ frontend's JobList::TICK_INTERVAL: The
     frontend should still be able to update the screen etc every n-th tick.
     (This should be a much higher value (>=1000?) for non-interactive
     frontends.) */
  const unsigned MAX_CALLBACK_DURATION = 500000;

}

CachedUrl::CachedUrl(const string& filename, uint64 prio)
    : DataSource(), filenameVal(filename), priority(prio), progressVal(),
      file(0) {
  struct stat fileInfo;
  int status = stat(filename.c_str(), &fileInfo);
  Assert (status == 0); // Should be ensured by creator of object
  if (status == 0) progressVal.setDataSize(fileInfo.st_size);
}

CachedUrl::~CachedUrl() {
  active.erase(this);
  delete file;
}

const Progress* CachedUrl::progress() const { return &progressVal; }

const string& CachedUrl::location() const { return filenameVal; }

void CachedUrl::run() {
  debug("CachedUrl %1 run()", this);
  IOSOURCE_SEND(DataSource::IO, io,
                dataSource_dataSize, (progressVal.dataSize()));
  cont();
}

bool CachedUrl::paused() const {
  Set::const_iterator i = active.find(const_cast<CachedUrl*>(this));
  return (i == active.end());
}

void CachedUrl::pause() { active.erase(this); }

// Add this to active set, maybe register glib callback
void CachedUrl::cont() {
  active.insert(this);
  if (active.size() == 1 && spoolDataCallbackId == 0) {
    debug("Callback on");
    g_idle_add(&spoolDataCallback, 0);
  }
}

CachedUrl::Set CachedUrl::active;

int CachedUrl::spoolDataCallbackId = 0;

// Initially assume very slow access: 20kB/sec
unsigned CachedUrl::readSpeed = 20 << 10;

/* This function treats the set of active CachedUrls as a queue and keeps
   reading data from the first object. The difficult bit is that we have to
   try to do 2 contradicting things equally well:

   1) Always return before MAX_CALLBACK_DURATION microseconds are over - we
   want to avoid that the frontend appears to "hang".

   2) Read the data from the file in chunks which are as big as possible, for
   best speed. It is even conceivable that the file resides on NFS and that
   the available network bandwidth varies over time...

   Solution (imperfect, but more than sufficient in practice): Imitate TCP's
   slow start algorithm: Read in smaller chunks at first, then keep adjusting
   the size depending on the measured read speed. */
gboolean CachedUrl::spoolDataCallback(gpointer) {
  if (active.empty()) {
    debug("Callback off");
    spoolDataCallbackId = 0;
    return FALSE; // "Don't call me again"
  }

  // FIXME: Code below only lightly tested, probably buggy

  debug("Callback working");
  GTimeVal start;
  g_get_current_time(&start);

  const unsigned BUFSIZE = 256 << 10;
  ArrayAutoPtr<byte> bufDel(new byte[BUFSIZE]);
  byte* buf = bufDel.get();

  unsigned left = MAX_CALLBACK_DURATION; // usecs left before timeout
  while (true) {
    CachedUrl* x = *active.begin();
    IOSource<DataSource::IO>& io = x->io;

    // Ensure file is open
    if (x->file == 0) {
      x->file = new bifstream(x->filenameVal.c_str(), ios::binary);
      if (!*x->file) {
        string err = subst(_("Could not open `%L1' for input: %L2"),
                           x->filenameVal, strerror(errno));
        IOSOURCE_SEND(DataSource::IO, io, job_failed, (err));
        active.erase(x);
        break;
      }
    }

    /* toRead = nr of bytes to read from file, such that "left" usecs pass
       during the read with an assumed speed of readSpeed. */
    unsigned toRead = uint64(readSpeed) * left / 1000000;
    if (toRead > BUFSIZE) toRead = BUFSIZE;
    readBytes(*x->file, buf, toRead);
    unsigned n = x->file->gcount();
    debug("  readSpeed %1 bytes/sec, %2 usecs left => reading %3 bytes",
          readSpeed, left, toRead);

    // Pass data to consumer
    uint64 currentSize = x->progressVal.currentSize() + n;
    x->progressVal.setCurrentSize(currentSize);
    IOSOURCE_SEND(DataSource::IO, io, dataSource_data, (buf, n, currentSize));

    if (x->file->eof()) {
      IOSOURCE_SEND(DataSource::IO, io, job_succeeded, ());
      active.erase(x);
      break;
    }
    if (!*(x->file)) {
      string err = subst(_("Could not read from `%L1': %L2"),
                         x->filenameVal, strerror(errno));
      IOSOURCE_SEND(DataSource::IO, io, job_failed, (err));
      active.erase(x);
      break;
    }

    GTimeVal nowTime;
    g_get_current_time(&nowTime);
    // now = usecs since start's value
    unsigned now = (nowTime.tv_sec - start.tv_sec) * 1000000
                   + nowTime.tv_usec - start.tv_usec;
    if (now + 50*1000 >= MAX_CALLBACK_DURATION) {
      // Out of time (or nearly so; allowing 50ms earlier), stop for now
      break;
    }

    // timeTaken = usecs it took to read n bytes
    unsigned timeTaken = now + left - MAX_CALLBACK_DURATION;
    unsigned newSpeed = uint64(n) * 1000000 / timeTaken;
    // At most double or halve the readSpeed
    if (newSpeed < readSpeed / 2) readSpeed /= 2;
    else if (newSpeed > readSpeed * 2) readSpeed *= 2;
    else readSpeed = newSpeed;
    debug("  Got %1 bytes in %2 usec (%3 bytes/sec), new readSpeed %4 "
          "bytes/sec", n, timeTaken, newSpeed, readSpeed);

    left = MAX_CALLBACK_DURATION - now;

  } // endwhile (true)

  return TRUE;
}

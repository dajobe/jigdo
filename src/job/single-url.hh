/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2002-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Single HTTP or FTP retrievals
  More low-level code which interfaces with libwww is in download.cc
  Higher-level GUI code is in gtk-single-url.cc

*/

#ifndef SINGLE_URL_HH
#define SINGLE_URL_HH

#include <job.hh>
#include <download.hh>
#include <nocopy.hh>
#include <progress.hh>
//______________________________________________________________________

namespace Job {
  class SingleUrl;
}

/** Class which handles downloading a HTTP or FTP URL. It is a layer on top
    of Download and provides some additional functionality:
    <ul>

      <li>Maintains a Progress object (for "time remaining", "kB/sec")

      <li>Does clever resumes of downloads (with partial overlap to check
      that the correct file is being resumed)

      <li>Does pause/continue by registering a callback function with glib.
      This is necessary because Download::pause() must not be called from
      within download_data()

      <li>Contains a state machine which handles resuming the download a
      certain number of times if the connection is dropped.

    </ul>*/
class Job::SingleUrl : NoCopy, private Download::Output {
public:
  class IO;

  /** Number of bytes to download again when resuming a download. These bytes
      will be compared with the old data. This value is never read by
      SingleUrl itself, it's just a hint for code using SingleUrl. */
  static const unsigned RESUME_SIZE = 64*1024;
  /** Number of times + 1 a download for the same URL will be resumed. A
      resume is necessary if the connection drops unexpectedly. */
  static const int MAX_TRIES = 20;
  /** Delay (millisec) before a download is resumed automatically, and during
      which a "resuming..." message or similar can be displayed. This value
      is never read by SingleUrl itself, it's just a hint for code using
      SingleUrl. */
  static const int RESUME_DELAY = 3000;

  /** Create object and immediately start the download. If offset == 0, the
      download starts from the beginning, otherwise we resume the download
      from that offset.

      When resuming, the IO object will eventually be fed data starting from
      the given offset. However, before that, we download the preceding
      bufLen bytes again (i.e. offsets [offset-bufLen;offset) ) and compare
      them with the data supplied in buf. If there is a mismatch, the file
      changed on the server and IO::job_failed() is called. If a resume is
      not possible (e.g. server does not support HTTP ranges), there's also
      an error.

      @param buf An ARRAY WHICH MUST HAVE BEEN ALLOCATED WITH new[]
      containing bufLen bytes of previously downloaded data. Will be freed
      automatically. Can be null if bufLen is also 0.
      @param bufLen length of buf. An assertion will trigger and you will be
      slapped over the head if bufLen>offset */
  SingleUrl(IO* ioPtr, const string& uri, uint64 offset = 0,
            const byte* buf = 0, size_t bufLen = 0,
            bool pragmaNoCache = false);
  virtual ~SingleUrl();

  /** This class does not have a public io member because this interferes
      with the way its derived classes (e.g. JigdoDownload) work. To access
      the correct io object, use this method. */
  virtual IOPtr<SingleUrl::IO>& io();
  virtual const IOPtr<SingleUrl::IO>& io() const;

  /** Current try - possible values are 1..MAX_TRIES (inclusive) */
  inline int currentTry() const;

  /** Is the download currently paused? */
  bool paused() const;
  /** Pause the download. */
  void pause();
  /** Continue downloading. */
  void cont();
  /** Stop download. */
  inline void stop();

  /** Are we in the process of resuming, i.e. are we currently downloading
      data before the resume offset and comparing it? */
  inline bool resuming() const;

  /** Did the download fail with an error? */
  inline bool failed() const;

  /** Is download finished? (Also returns true if FTP/HTTP1.0 connection
      dropped) */
  inline bool succeeded() const;

  /** Return the internal progress object */
  inline const Progress* progress() const;

  /** Call this after the download has failed (and your job_failed() has been
      called), to check whether resuming the download is possible. Returns
      true if resume is possible, i.e. error was not permanent and maximum
      number of tries not exceeded. */
  inline bool resumePossible() const;

  /** "Manually" cause resumePossible() to return false from now on. Useful
      e.g. if the user of this class wanted to write to a file and there was
      an error doing so. */
  inline void setNoResumePossible();

  /** After resumePossible() returned true, call this to resume the download.
      Buf and buflen must be set up as described above, the first byte in buf
      must be the byte at offset (progress()->currentSize() - bufLen). bufLen
      can be 0; in this case, buf can be null too. */
  void resume(const byte* buf, size_t bufLen);

private:
  IOPtr<IO> ioVal; // Points to e.g. a GtkSingleUrl

  // Virtual methods from Download::Output
  virtual void download_dataSize(uint64 n);
  virtual void download_data(const byte* data, size_t size,
                             uint64 currentSize);
  virtual void download_succeeded();
  virtual void download_failed(string* message);
  virtual void download_message(string* message);

  Download download; // download.run() called by ctor
  Progress progressVal;

  // Register resumeFailed_callback
  inline void resumeFailed();

  /* A callback function which is registered if the resume needs to be
     aborted. It'll get executed the next time the main glib loop is
     executed. This delayed execution is necessary because libwww doesn't
     like Download::stop() being called from download_newData(). */
  static gboolean resumeFailed_callback(gpointer data);
  unsigned int resumeFailedId; // GTK idle function id, or 0 if none

  // All 3 are null if we're not / no longer resuming
  const byte* resumeStart; // ptr to start of buffer
  const byte* resumePos; // current pos in buffer
  const byte* resumeEnd; // ptr to first byte after buffer

  int tries; // Nr of tries resuming after interrupted connection
};
//______________________________________________________________________

/** User interaction for SingleUrl. */
class Job::SingleUrl::IO : public Job::IO {
public:

  /** Called by the job when it is deleted or when a different IO object is
      registered with it. If the IO object considers itself owned by its job,
      it can delete itself. */
  virtual void job_deleted() = 0;

  /** Called when the job has successfully completed its task. */
  virtual void job_succeeded() = 0;

  /** Called when the job fails. The only remaining action after getting this
      is to delete the job object. */
  virtual void job_failed(string* message) = 0;

  /** Informational message. */
  virtual void job_message(string* message) = 0;

  /** Called as soon as the size of the downloaded data is known. May not be
      called at all if the size is unknown.
      Problem with libwww: Returns size as long int - 2 GB size limit! */
  virtual void singleURL_dataSize(uint64 n) = 0;

  /** Called during download whenever data arrives, with the data that just
      arrived. You can write the data to a file, copy it away etc.
      currentSize is the offset into the downloaded data (including the
      "size" new bytes) - useful for "x% done" messages. */
  virtual void singleURL_data(const byte* data, size_t size,
                              uint64 currentSize) = 0;
};
//======================================================================

int Job::SingleUrl::currentTry() const { return tries; }
bool Job::SingleUrl::resuming() const { return resumePos != resumeEnd; }
bool Job::SingleUrl::failed() const { return download.failed(); }
bool Job::SingleUrl::succeeded() const { return download.succeeded(); }
const Progress* Job::SingleUrl::progress() const { return &progressVal; }
bool Job::SingleUrl::resumePossible() const {
  if (tries >= MAX_TRIES || !download.interrupted()) return false;
  if (progressVal.currentSize() == 0) return true;
  return (progressVal.dataSize() > 0
          && progressVal.currentSize() < progressVal.dataSize());
}
void Job::SingleUrl::setNoResumePossible() {
  tries = MAX_TRIES; // Download failed permanently, do not resume
}
void Job::SingleUrl::stop() {
  download.stop();
}

#endif

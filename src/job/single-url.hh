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

#include <bstream-counted.hh>
#include <datasource.hh>
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

    </ul>

    This one will forever remain single since there are no single parties
    around here and it's rather shy. TODO: If you pity it too much, implement
    a MarriedUrl. */
class Job::SingleUrl : public Job::DataSource, private Download::Output {
public:
  /** Number of bytes to download again when resuming a download. These bytes
      will be compared with the old data. */
  static const unsigned RESUME_SIZE = 16*1024;
  /** Number of times + 1 a download for the same URL will be resumed. A
      resume is necessary if the connection drops unexpectedly. */
  static const int MAX_TRIES = 20;
  /** Delay (millisec) before a download is resumed automatically, and during
      which a "resuming..." message or similar can be displayed. This value
      is never read by SingleUrl itself, it's just a hint for code using
      SingleUrl. */
  static const int RESUME_DELAY = 3000;

  /** Create object, but don't start the download yet - use run() to do that.
      @param uri URI to download */
  SingleUrl(/*IOPtr DataSource::IO* ioPtr, */const string& uri);
  virtual ~SingleUrl();

  /** Set offset to resume from - download must not yet have been started,
      call before run(). By default, the offset is 0 after object creation
      and after a download has finished. If resumeOffset>0, SingleUrl will
      first read RESUME_SIZE bytes from destStream at offset
      destOffset+resumeOffset-RESUME_SIZE (or less if the first read byte
      would be <destOffset otherwise). These bytes are compared to bytes
      downloaded and *not* passed on to the IO object.
      @param resumeOffset 0 to start download from start. Otherwise,
      destStream[destOffset;destOffset+resumeOffset) is expected to contain
      data from an earlier, partial download. The last up to RESUME_SIZE
      bytes of these will be read from the file and compared to newly
      downloaded data. */
  void setResumeOffset(uint64 resumeOffset);

  /** Behaviour as above. Defaults if not called before run() is (0,0,0).
      @param destStream Stream to write downloaded data to, or null. Is *not*
      closed from the dtor!
      @param destOffset Offset of URI's data within the file. 0 for
      single-file download, >0 if downloaded data is to be written somewhere
      inside a bigger file.
      @param destEndOffset Offset of first byte in destStream (>destOffset)
      which the SingleUrl is not allowed to overwrite. 0 means don't care, no
      limit. */
  void setDestination(BfstreamCounted* destStream,
                      uint64 destOffset, uint64 destEndOffset);

  /** Behaviour as above. Defaults if not called before run() is false, i.e.
      don't add "Pragma: no-cache" header.
      @param pragmaNoCache If true, perform a "reload", discarding anything
      cached e.g. in a proxy. */
//   inline void setPragmaNoCache(bool pragmaNoCache);

  /** Start download or resume it

      All following bytes are written to destStream as well as passed to the
      IO object. We seek to the correct position each time when writing, so
      several parallel downloads for the same destStream are possible.

      An error is raised if you want to resume but the server doesn't support
      it, or if the server wants to send more data than expected, i.e. the
      byte at destEndOffset inside destStream would be overwritten, or if
      destEndOffset is set and the size of the data as reported by the server
      is not destEndOffset-destOffset. */
  virtual void run();

  /** Current try - possible values are 1..MAX_TRIES (inclusive) */
  inline int currentTry() const;

  /** Is the download currently paused? From DataSource. */
  virtual bool paused() const;
  /** Pause the download. From DataSource. */
  virtual void pause();
  /** Continue downloading. From DataSource. */
  virtual void cont();
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

  /** Return the internal progress object. From DataSource. */
  virtual const Progress* progress() const;
  /** Return the URL used to download the data. From DataSource. */
  virtual const string& location() const;

  /** Return the registered destination stream, or null */
  inline BfstreamCounted* destStream() const;

  /** Set destination stream, can be null */
  //inline void setDestStream(bfstream* destStream);

  /** Call this after the download has failed (and your job_failed() has been
      called), to check whether resuming the download is possible. Returns
      true if resume is possible, i.e. error was not permanent and maximum
      number of tries not exceeded. */
  inline bool resumePossible() const;

  /** "Manually" cause resumePossible() to return false from now on. Useful
      e.g. if the user of this class wanted to write to a file and there was
      an error doing so. */
  inline void setNoResumePossible();

private:
  // Virtual methods from Download::Output
  virtual void download_dataSize(uint64 n);
  virtual void download_data(const byte* data, unsigned size,
                             uint64 currentSize);
  virtual void download_succeeded();
  virtual void download_failed(string* message);
  virtual void download_message(string* message);

  /** Stop download from idle callback. */
  //void stopLater();

  /* Write bytes at specified offset. Return FAILURE and call
     io->job_failed() if error during writing or if written data would
     exceed destEndOff. */
  inline bool writeToDestStream(uint64 off, const byte* data, unsigned size);

  Download download;
  Progress progressVal;

  // Call io->job_failed(), then stopLater()
  inline void resumeFailed();

  SmartPtr<BfstreamCounted> destStreamVal;
  uint64 destOff, destEndOff;
  unsigned resumeLeft; // >0: Nr of bytes of resume overlap left

  /* Was setResumeOffset()/setDestination()/setPragmaNoCache() called before
     run()? If false, run() will call it with default values. */
  bool haveResumeOffset, haveDestination; //, havePragmaNoCache;

  int tries; // Nr of tries resuming after interrupted connection
};
//======================================================================


// void Job::SingleUrl::setPragmaNoCache(bool pragmaNoCache) {
//   download.setPragmaNoCache(pragmaNoCache);
//   havePragmaNoCache = true;
// }
int Job::SingleUrl::currentTry() const { return tries; }
bool Job::SingleUrl::resuming() const { return resumeLeft > 0; }
bool Job::SingleUrl::failed() const { return download.failed(); }
bool Job::SingleUrl::succeeded() const { return download.succeeded(); }
BfstreamCounted* Job::SingleUrl::destStream() const {
  return destStreamVal.get(); }

bool Job::SingleUrl::resumePossible() const {
//   msg("Job::SingleUrl::resumePossible tries=%1 interr=%2 curSiz=%3",
//       tries, download.interrupted(), progressVal.currentSize());
  if (tries >= MAX_TRIES || !download.interrupted()) return false;
  if (progressVal.currentSize() == 0
      || progressVal.dataSize() == 0) return true;
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

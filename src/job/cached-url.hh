/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

  Spool data from cache file

  A CachedUrl is started when MakeImageDl::childFor() was instructed to
  return a DataSource for an URL/md5sum, but that data was already present on
  the local disc.

*/

#ifndef CACHED_URL_HH
#define CACHED_URL_HH

#include <config.h>

#include <glib.h>
#include <set>

#include <bstream.hh>
#include <datasource.hh>
#include <nocopy.hh>
#include <progress.hh>
//______________________________________________________________________

namespace Job {
  class CachedUrl;
}

class Job::CachedUrl : public Job::DataSource {
public:
  /** Create object, but don't start outputting data yet - use run() to do
      that.
      @param filename File to spool from
      @param prio "Priority" - if >1 CachedUrls are running, the ones with
      lower prio get spooled first. */
  CachedUrl(const string& filename, uint64 prio);
  virtual ~CachedUrl();

  virtual void run();

  /** Is the download currently paused? From DataSource. */
  virtual bool paused() const;
  /** Pause the download. From DataSource. */
  virtual void pause();
  /** Continue downloading. From DataSource. */
  virtual void cont();


  /** Return the internal progress object. From DataSource. */
  virtual const Progress* progress() const;
  /** Return the URL used to download the data. From DataSource. */
  virtual const string& location() const;

  //inline const string& filename() const;

private:
  /* Set of all active (non-paused) downloads. After construction, CachedUrls
     are initially paused, run() and cont() are identical. */
  struct Cmp {
    inline bool operator()(const CachedUrl* a, const CachedUrl* b) const;
  };
  friend struct Cmp;
  typedef set<CachedUrl*, Cmp> Set;
  static Set active;
  static unsigned readSpeed; // Bytes per sec read from active.front()->file

  // glib callback, spools data when main loop is otherwise idle.
  static gboolean spoolDataCallback(gpointer);
  static int spoolDataCallbackId; // glib event source ID for above

  string filenameVal;
  uint64 priority;
  Progress progressVal;
  bifstream* file;
};
//______________________________________________________________________

//const string& Job::CachedUrl::filename() const { return filenameVal; }

bool Job::CachedUrl::Cmp::operator()(const CachedUrl* a, const CachedUrl* b)
    const {
  if (a->priority == b->priority) return (a < b);
  else return (a->priority < b->priority);
}

#endif

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Statistics: How many % of download done, average speed, ETA

*/

#ifndef PROGRESS_HH
#define PROGRESS_HH

#include <config.h>

#include <iosfwd>
#include <string>
#include <glib.h>

#include <progress.fh>
//______________________________________________________________________

class Progress {
public:
  inline Progress();
  inline ~Progress();

  /** Set amount of data fetched so far */
  inline void setCurrentSize(uint64 x);
  /** Read amount of data fetched so far */
  inline uint64 currentSize() const;
  /** Set total amount of data - default is 0 (unknown) */
  inline void setDataSize(uint64 x);
  /** Read total amount of data - default is 0 (unknown) */
  inline uint64 dataSize() const;

  /** Call this at regular intervals: At least every SPEED_TICK_INTERVAL
      milliseconds, but can also be more often, preferably in such a way that
      every nth call happens every SPEED_TICK_INTERVAL milliseconds.
      @param now Current time: {GTimeVal now;g_get_current_time(&now);} Use
      same "now" value for related calls to timeLeft(), tick() etc.
      @param millisecs Number of milliseconds elapsed since last call to this
      function (approximate) */
  void tick(const GTimeVal& now, int millisecs);
  /** Register/unregister a callback function with glib which automatically
      calls tick() every SPEED_TICK_INTERVAL milliseconds. This only
      registers one function which traverses the list of all Progress objects
      with autoTick==true.
      ~Progress automatically unregisters the autotick callback if necessary.
      @param enable true to call tick() automatically, false to stop calling
      it. */
  void setAutoTick(bool enable);
  /** Is autotick enabled for this object? */
  inline bool autoTick();
  /** Return estimated bytes/sec for (roughly) last few secs (-1 for
      unknown) */
  int speed(const GTimeVal& now) const;
  /** Return estimated remaining time in seconds (-1 for unknown). Call
      *before* tick() or you'll throw away some accuracy. */
  int timeLeft(const GTimeVal& now) const;

  /** Append to s something like "9999B", "9999kB", "9999MB", "99.9MB" */
  static void appendSize(string* s, uint64 size);
  /** Append "50kB" if size not known, else "50kB of 10MB" */
  static void appendSizes(string* s, uint64 size, uint64 total);
  /** Convenience method: Create long progress string like "50%, 50kB of
      100kB (3333 of 6666 bytes)" */
  void appendProgress(string* s) const;
  /** Convenience method: Create long string with speed and estimated time of
      arrival like "00:01:59 remaining, 100kB/sec" */
  void appendSpeed(string* s, int speed, int timeLeft) const;

  /** Reset internal state of "time left" calculation. Does not touch
      currentSize or dataSize. Use this e.g. when continuing a download after
      it has been paused. Also records the current time to signify the start
      of the download, so don't wait too long between calling this and
      actually (re)starting the download.
      Important: Use setCurrentSize(0);reset(); and not the other way round,
      or the speed calculation will go belly up. */
  void reset();

  static const int SPEED_TICK_INTERVAL = 3000;

  ostream& put(ostream& s) const;

private:
  // Bytes downloaded so far
  uint64 currentSizeVal;
  // Total bytes, or 0 for don't know
  uint64 dataSizeVal;

  /* Estimation of download speed for "x kBytes per sec" display: Basic idea
     is to calculate the average download rate for the last 30 secs
     (SPEED_TICK_INTERVAL/1000*SPEED_SLOTS) all the time. To do this, we need
     to take note at regular intervals (SPEED_TICK_INTERVAL) of how much
     we've downloaded so far, and store the value for later use in one of
     SPEED_SLOTS slots. The slots are a "ring buffer" - the oldest value is
     always overwritten by the newest. */
  static const int SPEED_SLOTS = 10;
  // If speed grows by >=x% from newer slot to older, ignores older slots
  static const unsigned SPEED_MAX_GROW = 130;
  static const unsigned SPEED_MIN_SHRINK = 70;
  uint64 slotSizeVal[SPEED_SLOTS]; // Value of currentSizeVal at slot start
  GTimeVal slotStart[SPEED_SLOTS]; // Timestamp of start of slot
  int currSlot; // Rotates through 0..SPEED_SLOTS-1
  int calcSlot; // Index of slot which is used for speed calculation
  int currSlotLeft; // Millisecs before a new slot is started
  /* Secs that timeLeft() must have *increased* by before the new
     value is reported. A *drop* of timeLeft() values is reported
     immediately. */
  static const int TIME_LEFT_GROW_THRESHOLD = 5;
  mutable int prevTimeLeft;

  /* Prev/next in list of autotick objects, or either one null if at
     start/end of list, or both null if not in list, or both null if only
     object in list. Yeah, keep it nice and simple! */
  Progress* prev;
  Progress* next;
  // Callback function which calls tick() on the objects
  static gboolean autoTickCallback(gpointer);
  // First object in list of autotick objects, or null if list empty
  static Progress* autoTickList;
  static int autoTickId; // ID of glib event source for autoTickCallback
};

inline ostream& operator<<(ostream& s, const Progress& r);
//______________________________________________________________________

Progress::Progress()
  : currentSizeVal(0), dataSizeVal(0), prev(0), next(0) { reset(); }
Progress::~Progress() { setAutoTick(false); }
void   Progress::setCurrentSize(uint64 x) { currentSizeVal = x; }
uint64 Progress::currentSize() const { return currentSizeVal; }
void   Progress::setDataSize(uint64 x) { dataSizeVal = x; }
uint64 Progress::dataSize() const { return dataSizeVal; }
ostream& operator<<(ostream& s, const Progress& r) { return r.put(s); }
bool Progress::autoTick() {
  return prev != 0 || next != 0 || autoTickList == this;
}

#endif

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Statistics: How many % of download done, average speed, ETA

  A "bug": If you pause and then resume the download after a while, speed()
  will return values which are unusually high, higher than your bandwidth. I
  think this is because of the way jigdo pauses its downloads; just by not
  select()ing the connection's buffers. I suspect this means that quite a lot
  of data queues up in the OS's buffers, and this data is then delivered to
  us in one big chunk when we resume.

*/

#include <config.h>

#include <stdio.h>
#include <glib.h>
#include <iostream>

#include <debug.hh>
#include <progress.hh>
#include <string-utf.hh>

#ifndef DEBUG_PROGRESS
#  define DEBUG_PROGRESS (DEBUG && 1)
#endif
//______________________________________________________________________

/** Append to s something like "9999B", "9999kB", "9999MB", "99.9MB" */
void Progress::appendSize(string* s, uint64 size) {
  if (size < 10000) {
    append(*s, size).append(_("B"));
    return;
  }
  if (size < 1024 * 100) {
    append(*s, static_cast<double>(size) / 1024.0).append(_("kB"));
    return;
  }
  if (size < 1024 * 10000) {
    append(*s, size / 1024).append(_("kB"));
    return;
  }
  if (size < 1048576 * 100) {
    append(*s, static_cast<double>(size) / 1048576.0).append(_("MB"));
    return;
  }
  append(*s, size / 1048576).append(_("MB"));
  return;
}
//________________________________________

void Progress::appendSizes(string* s, uint64 size, uint64 total) {
  if (total <= size) {
    total = size;
    appendSize(s, size);
  } else {
    //append(s, 100.0 * size / total).append("%, ");
    appendSize(s, size);
    *s += _(" of ");
    appendSize(s, total);
  }
}
//________________________________________

void Progress::appendProgress(string* s) const {
  if (dataSizeVal == 0) {
    appendSize(s, currentSizeVal);
  } else {
    append(*s, 100.0 * currentSizeVal / dataSizeVal).append(_("%, "));
    appendSize(s, currentSizeVal);
    *s += _(" of ");
    appendSize(s, dataSizeVal);
  }
  *s += " (";
  append(*s, currentSizeVal);
  if (dataSizeVal > 0) {
    *s += _(" of ");
    append(*s, dataSizeVal);
  }
  *s += _(" bytes)");
}
//________________________________________

void Progress::appendSpeed(string* s, int speed, int timeLeft) const {
  if (timeLeft >= 0) {
    const int BUF_LEN = 64;
    static char buf[BUF_LEN];
    int hr = timeLeft / 3600;
    snprintf(buf, BUF_LEN, _("%02d:%02d:%02d remaining"),
             hr,  timeLeft / 60 - hr * 60, timeLeft % 60);
    buf[BUF_LEN - 1] = '\0';
    *s += buf;
  }
  if (speed >= 0) {
    if (!(*s).empty()) *s += _(", ");
    appendSize(s, speed);
    *s += _("/sec");
  }
}
//______________________________________________________________________

void Progress::reset() {
  currSlot = 0;
  calcSlot = 0;
  currSlotLeft = SPEED_TICK_INTERVAL;
  prevTimeLeft = -TIME_LEFT_GROW_THRESHOLD;
  for (int i = 0; i < SPEED_SLOTS; ++i) {
    slotStart[i].tv_sec = slotStart[i].tv_usec = 0;
    slotSizeVal[i] = 0;
  }
  //startTime.tv_sec = 0;
  g_get_current_time(&slotStart[0]);
  slotSizeVal[0] = currentSize();
}
//______________________________________________________________________

/* The way the speed of the connection is measured is quite complicated,
   because it attempts to do two things equally well: 1) Show the *average*
   throughput accurately even on bursty connections, 2) React quickly when
   data suddenly stops flowing. */
void Progress::tick(const GTimeVal& now, int millisecs) {
//   if (slotStart[calcSlot].tv_sec == 0)
//     slotStart[calcSlot] = now;

  currSlotLeft -= millisecs;
  if (currSlotLeft > 0) return;

  // New slot starts now
  /* Compare the throughput of the slot that was just finished (at
     [currSlot]) with the next older one. If speed between these two differs
     too much (i.e. sudden large burst of data / data stops flowing), set
     calcSlot=currSlot, i.e. from now on calculate speed from youngest slot
     and ignore older slots. */
  int oldSlot = currSlot - 1;
  if (oldSlot < 0) oldSlot = SPEED_SLOTS - 1;
  if (slotStart[oldSlot].tv_sec != 0) {
    uint64 slotSize = currentSizeVal - slotSizeVal[currSlot];
    uint64 oldSlotSize = slotSizeVal[currSlot] - slotSizeVal[oldSlot];
    uint64 sizeChange = 100 * slotSize / (oldSlotSize + 1);
    //cerr << "sizeChange=" << sizeChange << " slotSize=" << slotSize
    //     << " oldSlotSize=" << oldSlotSize << endl;
    if (sizeChange >= SPEED_MAX_GROW || sizeChange <= SPEED_MIN_SHRINK)
      calcSlot = currSlot; // Speed differs too much
  }

  // Increase currSlot, overwriting oldest slot
  //cerr << "currSlot=" << currSlot << " oldSlot=" << oldSlot
  //     << " calcSlot=" << calcSlot << endl;
  currSlotLeft += SPEED_TICK_INTERVAL;
  if (++currSlot == SPEED_SLOTS) currSlot = 0;
  // Init new slot's entries
  slotSizeVal[currSlot] = currentSizeVal;
  slotStart[currSlot] = now;

  // If calcSlot was oldest slot, increase it so it's the oldest again
  if (currSlot == calcSlot)
    if (++calcSlot == SPEED_SLOTS) calcSlot = 0;
}
//______________________________________________________________________

int Progress::speed(const GTimeVal& now) const {
  //if (slotStart[calcSlot].tv_sec == 0) return -1;
  double elapsed = (now.tv_sec - slotStart[calcSlot].tv_sec) * 1000000.0;
  if (now.tv_usec >= slotStart[calcSlot].tv_usec)
    elapsed += now.tv_usec - slotStart[calcSlot].tv_usec;
  else
    elapsed -= slotStart[calcSlot].tv_usec - now.tv_usec;
  if (elapsed == 0.0) return -1;
//   cerr<<"speed: "<<currentSizeVal - slotSizeVal[calcSlot]<<" bytes in "
//       <<elapsed / 1000000.0<<" sec, current size "<<currentSizeVal<<endl;
  int speed = static_cast<int>(
      static_cast<double>(currentSizeVal - slotSizeVal[calcSlot])
      / elapsed * 1000000.0);
  return speed;
}
//________________________________________

/* In contrast to the numbers for speed measurement, do not "reset" the
   calculation slot e.g. if amount of data changes rapidly from one tick to
   the next. Instead, always try to use the oldest slot and only fall back to
   an earlier one if the download hasn't been running for long enough. */
int Progress::timeLeft(const GTimeVal& now) const {
  if (dataSizeVal <= currentSizeVal) return -1;
  int oldestSlot = currSlot + 1;
  if (oldestSlot == SPEED_SLOTS) oldestSlot = 0;
  while (slotStart[oldestSlot].tv_sec == 0)
    if (++oldestSlot == SPEED_SLOTS) oldestSlot = 0;
  //if (slotStart[oldestSlot].tv_sec == 0) oldestSlot = calcSlot;
  //if (slotStart[oldestSlot].tv_sec == 0) return -1;

  double elapsed = (now.tv_sec - slotStart[oldestSlot].tv_sec)
                   * 1000000.0;
  if (elapsed == 0.0) return -1;
  if (now.tv_usec >= slotStart[oldestSlot].tv_usec)
    elapsed += now.tv_usec - slotStart[oldestSlot].tv_usec;
  else
    elapsed -= slotStart[oldestSlot].tv_usec - now.tv_usec;
  double speed = (currentSizeVal - slotSizeVal[oldestSlot])
                 / elapsed * 1000000.0;
  if (speed == 0.0) return -1;
  int remaining = static_cast<uint32>(
                  (dataSizeVal - currentSizeVal) / speed);
  /* Avoid sudden and frequent jumps in the reported time by not reporting a
     change if the newly calculated remaining time is a little higher than
     the previous one. */
  if (remaining >= prevTimeLeft
      && remaining < prevTimeLeft + TIME_LEFT_GROW_THRESHOLD)
    return prevTimeLeft;
  prevTimeLeft = remaining;
  return remaining;
}
//______________________________________________________________________

ostream& Progress::put(ostream& s) const {
  static long startSec = 0;
  if (startSec == 0) {
    GTimeVal now;
    g_get_current_time(&now);
    startSec = now.tv_sec;
  }

  for (int i = 0; i < SPEED_SLOTS; ++i) {
    s << ' ';
    if (i == currSlot) s << "curr:";
    if (i == calcSlot) s << "calc:";
    if (slotStart[i].tv_sec == 0)
      s << '?';
    else
      s << slotStart[i].tv_sec - startSec;
    s << '/' << slotSizeVal[i];
  }
  return s;
}
//______________________________________________________________________

Progress* Progress::autoTickList = 0;
int Progress::autoTickId = 0;

void Progress::setAutoTick(bool enable) {
  if (autoTick() == enable) return; // No change in state

  if (enable) {
    // Have tick() called automatically for this object
    Paranoid(prev == 0 && next == 0);
    if (autoTickList != 0) {
      Paranoid(autoTickList->prev == 0);
      next = autoTickList;
      next->prev = this;
    } else {
      // First entry - register callback
      autoTickId = g_timeout_add(SPEED_TICK_INTERVAL, autoTickCallback, 0);
#     if DEBUG_PROGRESS
      cerr << "Progress::setAutoTick: registered" << endl;
#     endif
    }
    autoTickList = this;

  } else {
    // Stop calling tick() automatically for this object
    if (prev == 0)
      autoTickList = next;
    else
      prev->next = next;
    if (next != 0)
      next->prev = prev;
    prev = next = 0;
    if (autoTickList == 0) {
      // This was the last entry - unregister callback
      g_source_remove(autoTickId);
      autoTickId = 0;
#     if DEBUG_PROGRESS
      cerr << "Progress::setAutoTick: unregistered" << endl;
#     endif
    }
  }
}

gboolean Progress::autoTickCallback(gpointer) {
# if DEBUG_PROGRESS
  cerr << "Progress::autoTickCallback";
# endif
  GTimeVal now;
  g_get_current_time(&now);
  Progress* p = autoTickList;
  while (p != 0) {
#   if DEBUG_PROGRESS
//     cerr << "  " << p << ": " << *p << endl;
    cerr << '.';
#   endif
    p->tick(now, SPEED_TICK_INTERVAL);
    p = p->next;
  }
# if DEBUG_PROGRESS
  cerr << endl;
# endif
  return TRUE; // "Call me again"
}

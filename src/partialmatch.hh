/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2002  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Helper class for mktemplate - queue of partially matched files

*/

#ifndef PARTIALMATCH_HH
#include <debug.hh>
#define PARTIALMATCH_HH

#ifndef INLINE
#  ifdef NOINLINE
#    define INLINE
#    else
#    define INLINE inline
#  endif
#endif

/** One object for each offset in image where any file /might/ match. Class
    interface is tailored towards mktemplate's needs, hence the unusual
    methods... */
class MkTemplate::PartialMatch {
  friend class MkTemplate::PartialMatchQueue;
public:
  /** Offset in image at which this match starts */
  uint64 startOffset() const { return startOff; }
  void setStartOffset(uint64 o) { startOff = o; }

  /** Next value of off at which to finish() sum & compare */
  uint64 nextEvent() const { return nextEv; }
  /** Move x's position in the queue depending on the new value of its
      nextEvent. Uses linear search. */
  INLINE void setNextEvent(PartialMatchQueue* matches, uint64 newNextEvent);

  /** Offset in buf of start of current MD5 block */
  size_t blockOffset() const { return blockOff; }
  void setBlockOffset(size_t b) { blockOff = b; }

  /** Number of block in file, i.e. index into file()->sums[] */
  size_t blockNumber() const { return blockNr; }
  void setBlockNumber(size_t b) { blockNr = b; }

  /** File whose sums matched so far */
  FilePart* file() const { return filePart; }
  void setFile(FilePart* f) { filePart = f; }

  /** Next in list, or null */
  PartialMatch* next() const { return nextPart; }

private:
  PartialMatch() { } // Only to be instantiated by PartialMatchQueue
  uint64 startOff; // Offset in image at which this match starts
  uint64 nextEv; // Next value of off at which to finish() sum & compare
  size_t blockOff; // Offset in buf of start of current MD5 block
  size_t blockNr; // Number of block in file, i.e. index into file->sums[]
  FilePart* filePart; // File whose sums matched so far
  PartialMatch* nextPart;
  bool operator<=(const PartialMatch& x) {
    return nextEvent() <= x.nextEvent();
  }
};
//________________________________________

/** Queue of PartialMatch objects, always kept sorted by ascending
    nextEvent. This may seem to make many operations inefficient, but
    this sort order is best for the central mktemplate loop. */
class MkTemplate::PartialMatchQueue {
  friend class MkTemplate::PartialMatch;
public:
  inline PartialMatchQueue();

  bool empty() const { return head == 0; }

  bool full() const { return freeHead == 0; }

  /** If true is returned, the queue is not only full, findDropCandidate() is
      also going to always return 0 from now on. Intended to be used for
      optimisation: As long as crammed()==true, needn't even check for
      further prospective matches.
      The value returned by this function is set to true by
      findDropCandidate(), and set to false by erase(), eraseFront(),
      eraseStartOffsetLess() and setNextEvent(). (NB not by
      setStartOffset()) */
  //bool crammed() const { return crammedVal; }

  PartialMatch* front() const { return head; }

  /** Add a new entry to the front of the queue. The queue must not be full.
      The new entry has all members set to 0, including its startOffset().
      Use the setter methods to change this.
      @return new object at front() */
  INLINE PartialMatch* addFront();

  /** Return pointer to element with the lowest startOff value, or
      null if queue empty. */
  INLINE PartialMatch* lowestStartOffset() const;

  /** Return lowest nextEvent() of all queue entries, which is always
      the nextEvent() of the first queue entry. Queue must not be
      empty. */
  INLINE uint64 nextEvent() const;

  /** Return first matching entry with startOffset()==off, or null if
      none found. */
  INLINE PartialMatch* findStartOffset(uint64 off) const;

  /** Return entry in list with lowest startOffset() value. List must not be
      empty. */
  INLINE PartialMatch* findLowestStartOffset() const;

  /** Remove all entries from list. */
  inline void erase();

  /** Remove first entry from list. List must not be empty. */
  INLINE void eraseFront();

  /** Remove all entries whose startOffset is strictly less than off */
  INLINE void eraseStartOffsetLess(uint64 off);

  /** If the queue is full, use some heuristics to find a PartialMatch in the
      queue which is "unlikely to lead to an actual match", or 0 if none
      exists.
      @param newStartOffset start offset of the new match which is to replace
      the object returned by this function. The heuristics favours offsets
      which are multiples of the assumed "sector size". */
  INLINE PartialMatch* findDropCandidate(unsigned* sectorLength,
                                         uint64 newStartOffset);

# if DEBUG
  void consistencyCheck() const;
# else
  void consistencyCheck() const { }
# endif

private:
  /* The size of the linked list of MD5 blocks awaiting matching must
     be limited for cases where there are lots of overlapping matches,
     e.g. both image and a file are all zeroes. */
  static const int MAX_MATCHES = 2048;
  PartialMatch* head; // First entry of queue, or null if queue empty
  PartialMatch* freeHead; // First elem of linked list of free slots, or null
  PartialMatch data[MAX_MATCHES];

  /* If true, queue is full, and furthermore findDropCandidate() will always
     return 0 */
  //bool crammedVal;
};
//______________________________________________________________________

void MkTemplate::PartialMatchQueue::erase() {
  head = 0;
  freeHead = &data[0];
  //crammedVal = false;
  // Put all elements in free list
  data[MAX_MATCHES - 1].nextPart = 0;
  for (int i = MAX_MATCHES - 2; i >= 0; --i)
    data[i].nextPart = &data[i + 1];
  consistencyCheck();
}

MkTemplate::PartialMatchQueue::PartialMatchQueue() {
  erase();
}
//______________________________________________________________________

#ifndef NOINLINE
#  include <partialmatch.ih> /* NOINLINE */
#endif

#endif

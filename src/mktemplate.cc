/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

  Create location list (.jigdo) and image template (.template)

  WARNING: This is very complicated code - be sure to run the "torture"
  regression test after making changes. Read this file from bottom to top.
  The .jigdo generating code is in mkjigdo.cc. The code for the queue of
  partial matches is in partialmatch.*.

  In the log messages that are output with --debug, uppercase means that it
  is finally decided what this area of the image is: UNMATCHED or a MATCH of
  a file.

*/

#include <config.h>

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <memory>

#include <autoptr.hh>
#include <compat.hh>
#include <debug.hh>
#include <log.hh>
#include <mimestream.hh>
#include <mkimage.hh>
#include <mktemplate.hh>
#include <scan.hh>
#include <string.hh>
#include <zstream-gz.hh>
#include <zstream-bz.hh>
//______________________________________________________________________

void MkTemplate::ProgressReporter::error(const string& message) {
  cerr << message << endl;
}
void MkTemplate::ProgressReporter::scanningImage(uint64) { }
void MkTemplate::ProgressReporter::matchFound(const FilePart*, uint64) { }
void MkTemplate::ProgressReporter::finished(uint64) { }

MkTemplate::ProgressReporter MkTemplate::noReport;
//______________________________________________________________________

MkTemplate::MkTemplate(JigdoCache* jcache, bistream* imageStream,
    JigdoConfig* jigdoInfo, bostream* templateStream, ProgressReporter& pr,
    int zipQuality, size_t readAmnt, bool addImage, bool addServers,
    bool useBzip2)
  : fileSizeTotal(0U), fileCount(0U), block(), readAmount(readAmnt),
    off(), unmatchedStart(), greedyMatching(true),
    cache(jcache),
    image(imageStream), templ(templateStream), zip(0),
    zipQual(zipQuality), reporter(pr), matches(new PartialMatchQueue()),
    sectorLength(),
    jigdo(jigdoInfo), addImageSection(addImage),
    addServersSection(addServers), useBzLib(useBzip2),
    matchExec() { }
//______________________________________________________________________

/* Because make-template should be debuggable even in non-debug builds,
   always compile in debug messages. */
#undef debug
Logger MkTemplate::debug("make-template");
//______________________________________________________________________

namespace {

  // Find the position of the highest set bit (e.g. for 0x20, result is 5)
  inline int bitWidth(uint32 x) {
    int r = 0;
    Assert(x <= 0xffffffff); // Can't cope with >32 bits
    if (x & 0xffff0000) { r += 16; x >>= 16; }
    if (x & 0xff00) { r += 8; x >>= 8; }
    if (x & 0xf0) { r += 4; x >>= 4; }
    if (x & 0xc) { r += 2; x >>= 2; }
    if (x & 2) { r += 1; x >>=1; }
    if (x & 1) r += 1;
    return r;
  }
  //________________________________________

  /* Avoid integer divisions: Modulo addition/subtraction, with
     certain assertions */
  // Returns (a + b) % m, if (a < m && b <= m)
  inline size_t modAdd(size_t a, size_t b, size_t m) {
    size_t r = a + b;
    if (r >= m) r -= m;
    Paranoid(r == (a + b) % m);
    return r;
  }
  // Returns (a - b) mod m, if (a < m && b <= m)
  inline size_t modSub(size_t a, size_t b, size_t m) {
    size_t r = a + m - b;
    if (r >= m) r -= m;
    Paranoid(r == (m + a - b) % m);
    return r;
  }
  // Returns (a + b) % m, if (a < m && -m <= b && b <= m)
  inline size_t modAdd(size_t a, int b, size_t m) {
    size_t r = a;
    if (b < 0) r += m;
    r += b;
    if (r >= m) r -= m;
    Paranoid(r == (a + static_cast<size_t>(b + m)) % m);
    return r;
  }
  //____________________

  /* Write part of a circular buffer to a Zobstream object, starting
     with offset "start" (incl) and ending with offset "end" (excl).
     Offsets can be equal to bufferLength. If both offsets are equal,
     the whole buffer content is written. */
  inline void writeBuf(const byte* const buf, size_t begin, size_t end,
                       const size_t bufferLength, Zobstream* zip) {
    Paranoid(begin <= bufferLength && end <= bufferLength);
    if (begin < end) {
      zip->write(buf + begin, end - begin);
    } else {
      zip->write(buf + begin, bufferLength - begin);
      zip->write(buf, end);
    }
  }
  //____________________

  // Write lower 48 bits of x to s in little-endian order
  void write48(bostream& s, uint64 x) {
#   if 0
    s << static_cast<byte>(x & 0xff)
      << static_cast<byte>((x >> 8) & 0xff)
      << static_cast<byte>((x >> 16) & 0xff)
      << static_cast<byte>((x >> 24) & 0xff)
      << static_cast<byte>((x >> 32) & 0xff)
      << static_cast<byte>((x >> 40) & 0xff);
#   else
    s.put(static_cast<byte>( x        & 0xff));
    s.put(static_cast<byte>((x >> 8)  & 0xff));
    s.put(static_cast<byte>((x >> 16) & 0xff));
    s.put(static_cast<byte>((x >> 24) & 0xff));
    s.put(static_cast<byte>((x >> 32) & 0xff));
    s.put(static_cast<byte>((x >> 40) & 0xff));
#   endif
  }

} // namespace
//______________________________________________________________________

/** Build up a template DESC section by appending items to a
    JigdoDescVec. Calls to descUnmatchedData() are allowed to accumulate, so
    that >1 consecutive unmatched data areas are merged into one in the DESC
    section. */
class MkTemplate::Desc {
public:
  Desc() : files(), offset(0) { }

  // Insert in DESC section: information about whole image
  inline void imageInfo(uint64 len, const MD5Sum& md5, size_t blockLength) {
    files.reserve((files.size() + 16) % 16);
    files.push_back(new JigdoDesc::ImageInfo(len, md5, blockLength));
  }
  // Insert in DESC section: info about some data that was not matched
  inline void unmatchedData(uint64 len) {
    JigdoDesc::UnmatchedData* u;
    if (files.size() > 0
        && (u = dynamic_cast<JigdoDesc::UnmatchedData*>(files.back())) !=0) {
      // Add more unmatched data to already existing UnmatchedData object
      u->resize(u->size() + len);
    } else {
      // Create new UnmatchedData object.
      files.reserve((files.size() + 16) % 16);
      files.push_back(new JigdoDesc::UnmatchedData(offset, len));
    }
    offset += len;
  }
  // Insert in DESC section: information about a file that matched
  inline void matchedFile(uint64 len, const RsyncSum64& r,
                          const MD5Sum& md5) {
    files.reserve((files.size() + 16) % 16);
    files.push_back(new JigdoDesc::MatchedFile(offset, len, r, md5));
    offset += len;
  }
  inline bostream& put(bostream& s, MD5Sum* md) {
    files.put(s, md);
    return s;
  }
private:
  JigdoDescVec files;
  uint64 offset;
};
//______________________________________________________________________

/* The following are helpers used by run(). It is usually not adequate
   to make such large functions inline, but there is only one call to
   them, anyway. */

inline bool MkTemplate::scanFiles(size_t blockLength, uint32 blockMask,
                                  size_t md5BlockLength) {
  bool result = SUCCESS;

  cache->setParams(blockLength, md5BlockLength);
  FileVec::iterator hashPos;

  for (JigdoCache::iterator file = cache->begin();
       file != cache->end(); ++file) {
    const RsyncSum64* sum = file->getRsyncSum(cache);
    if (sum == 0) continue; // Error - skip
    // Add file to hash list
    hashPos = block.begin() + (sum->getHi() & blockMask);
    hashPos->push_back(&*file);
  }
  return result;
}
//________________________________________

void MkTemplate::checkRsyncSumMatch2(const size_t blockLen,
    const size_t back, const size_t md5BlockLength, uint64& nextEvent,
    FilePart* file) {

  /* Don't schedule match if its startOff (== off - blockLen) is impossible.
     This is e.g. the case when file A is 1024 bytes long (i.e. is
     immediately matched once seen), and the first 1024 bytes of file B are
     equal to file A, and file B is in the image.
     [I think A gets matched, then the following line prevents that a partial
     match for B is also recorded.]
     Also prevents matches whose startOffset is <0, which could otherwise
     happen because rsum covers a blockLen-sized area of 0x7f bytes at the
     beginning. */
  if (off < unmatchedStart + blockLen) return;

  PartialMatch* x; // Ptr to new entry in "matches"
  if (matches->full()) {
    x = matches->findDropCandidate(&sectorLength, off - blockLen);
    if (x == 0 || x->file() == file) {
      /* If no more space left in queue and none of the entries in it is
         appropriate for discarding, just discard this possible file match!
         It's the only option if there are many, many overlapping matches,
         otherwise the program would get extremely slow. */
      debug(" %1: DROPPED possible %2 match at offset %3 (queue full)",
            off, file->leafName(), off - blockLen);
      return;
    }
    // Overwrite existing entry in the queue
    debug(" %1: DROPPED possible %2 match at offset %3 (queue full, match "
          "below replaces it)",
          off, x->file()->leafName(), x->startOffset());
  } else { // !matches->full()
    // Add new entry to the queue
    x = matches->addFront();
  }

  /* Rolling rsum matched - schedule an MD5Sum match. NB: In extreme cases,
     nextEvent may be equal to off */
  x->setStartOffset(off - blockLen);
  size_t eventLen = (file->size() < md5BlockLength ?
                     file->size() : md5BlockLength);
  x->setNextEvent(matches, x->startOffset() + eventLen);
  debug(" %1: Head of %2 match at offset %3, my next event %4",
        off, file->leafName(), x->startOffset(), x->nextEvent());
  if (x->nextEvent() < nextEvent) nextEvent = x->nextEvent();
  x->setBlockOffset(back);
  x->setBlockNumber(0);
  x->setFile(file);
}

/* Look for matches of sum (i.e. scanImage()'s rsum). If found, insert
   appropriate entry in "matches". */
void MkTemplate::checkRsyncSumMatch(const RsyncSum64& sum,
    const uint32& bitMask, const size_t blockLen, const size_t back,
    const size_t md5BlockLength, uint64& nextEvent) {

  typedef const vector<FilePart*> FVec;
  FVec& hashEntry = block[sum.getHi() & bitMask];
  if (hashEntry.empty()) return;

  FVec::const_iterator i = hashEntry.begin(), e = hashEntry.end();
  do {
    FilePart* file = *i;
    const RsyncSum64* fileSum = file->getRsyncSum(cache);
    if (fileSum != 0 && *fileSum == sum)
      // Insert new partial file match in "matches" queue
      checkRsyncSumMatch2(blockLen, back, md5BlockLength, nextEvent, file);
    ++i;
  } while (i != e);
  return;
}
//________________________________________

// Read the 'count' first bytes from file x and write them to zip
bool MkTemplate::rereadUnmatched(FilePart* file, uint64 count) {
  // Lower peak memory usage: Deallocate cache's buffer
  cache->deallocBuffer();

  ArrayAutoPtr<byte> tmpBuf(new byte[readAmount]);
  string inputName = file->getPath();
  inputName += file->leafName();
  auto_ptr<bistream> inputFile(new bifstream(inputName.c_str(),ios::binary));
  while (inputFile->good() && count > 0) {
    // read data
    readBytes(*inputFile, tmpBuf.get(),
              (readAmount < count ? readAmount : count));
    size_t n = inputFile->gcount();
    zip->write(tmpBuf.get(), n); // will catch Zerror "upstream"
    Paranoid(n <= count);
    count -= n;
  }
  if (count == 0) return SUCCESS;

  // error
  string err = subst(_("Error reading from `%1' (%2)"),
                     inputName, strerror(errno));
  reporter.error(err);
  return FAILURE;
}
//________________________________________

// Print info about a part of the input image
void MkTemplate::printRangeInfo(uint64 start, uint64 end, const char* msg,
                                const PartialMatch* x) {
  static const string empty;
  debug("[%1,%2) %3 %4", start, end, msg,
        (x != 0 ? x->file()->leafName() : empty) );
}

// oldAreaEnd != start, something's seriously wrong
void MkTemplate::debugRangeFailed() {
  static bool printed = false;
  cerr << "Assertion failed: oldAreaEnd == start" << endl;
  if (!printed) {
    cerr <<
      "You have found a bug in jigdo-file. The generated .template file is\n"
      "very likely broken! To help me find the bug, please rerun the\n"
      "command as follows:\n"
      "  [previous-command] --report=noprogress --debug=make-template >log 2>&1\n"
      "and send the _compressed_ `log' file to <jigdo" << "@atterer.net>."
         << endl;
    printed = true;
  }
}
//________________________________________

/* The block didn't match, so the whole file x doesn't match - re-read from
   file any data that is no longer buffered (and not covered by another
   match), and write it to the Zobstream. */
bool MkTemplate::checkMD5Match_mismatch(const size_t stillBuffered,
                                        PartialMatch* x, Desc& desc) {
  const PartialMatch* oldestMatch = matches->findLowestStartOffset();
  uint64 rereadEnd = off - stillBuffered;
  uint64 xStartOffset = x->startOffset();
  if (x != oldestMatch || xStartOffset >= rereadEnd) {
    // Everything still buffered, or there is another pending match
    matches->eraseFront(); // return x to free pool
    return SUCCESS;
  }

  /* Reread the right amount of data from the file for x, covering the image
     area from x->startOffset() to the first byte which is "claimed" by
     something else - either another match or the start of the still buffered
     data. */
  FilePart* xfile = x->file();
  Assert(matches->front() == x);
  matches->eraseFront(); // return x to free pool
  if (!matches->empty()) {
    // set rereadEnd to new lowest startOffset in matches
    const PartialMatch* newOldestMatch = matches->findLowestStartOffset();
    if (rereadEnd > newOldestMatch->startOffset())
      rereadEnd = newOldestMatch->startOffset();
  }
  debugRangeInfo(xStartOffset, rereadEnd,
                 "UNMATCHED after some blocks, re-reading from", x);

  desc.unmatchedData(rereadEnd - xStartOffset);
  unmatchedStart = rereadEnd;

  uint64 bytesToWrite = rereadEnd - xStartOffset;
  return rereadUnmatched(xfile, bytesToWrite);
}
//________________________________________

/* The file x was found in the image, and --match-exec is set. Set up env
   vars, run command.

   Why does this use system() instead of the "more secure" exec()? Because
   then things are actually easier to get right IMHO! My scenario is that
   someone has set up an env var with the destination path for the fallback,
   say $DEST, which might contain spaces. In that case, using exec() with
   some kind of "%label" substitution is awkward and error-prone - let's just
   have one single substitution scheme, that used by the shell! */
bool MkTemplate::matchExecCommands(PartialMatch* x) {
  Paranoid(!matchExec.empty());

  string matchPath, leaf;
  const string& leafName = x->file()->leafName();
  string::size_type lastSlash = leafName.rfind(DIRSEP);
  if (lastSlash == string::npos) {
    leaf = leafName;
  } else {
    matchPath.assign(leafName, 0, lastSlash + 1);
    leaf.assign(leafName, lastSlash + 1, string::npos);
  }
  Base64String md5Sum;
  md5Sum.write(x->file()->getMD5Sum(cache)->digest(), 16).flush();
  string file = x->file()->getLocation()->getPath();
  file += leafName;

  // Set environment vars
  if (compat_setenv("LABEL", x->file()->getLocation()->getLabel().c_str())
      || compat_setenv("LABELPATH", x->file()->getLocation()->getPath()
                       .c_str())
      || compat_setenv("MATCHPATH", matchPath.c_str())
      || compat_setenv("LEAF", leaf.c_str())
      || compat_setenv("MD5SUM", md5Sum.result().c_str())
      || compat_setenv("FILE", file.c_str())) {
    reporter.error(_("Could not set up environment for --match-exec "
                     "command"));
    return FAILURE;
  }

  // Execute command
  int status = system(matchExec.c_str());
  if (status == 0) return SUCCESS;
  reporter.error(_("Command supplied with --match-exec failed"));
  return FAILURE;
}
//________________________________________

/* Calculate MD5 for the previous md5ChunkLength (or less if at end of match)
   bytes. If the calculated checksum matches and it is the last MD5 block in
   the file, record a file match. If the i-th MD5Sum does not match, write
   the i*md5ChunkLength bytes directly to templ.
   @param stillBuffered bytes of image data "before current position" that
   are still in the buffer; they are at buf[data] to
   buf[(data+stillBuffered-1)%bufferLength] */
bool MkTemplate::checkMD5Match(byte* const buf,
    const size_t bufferLength, const size_t data,
    const size_t md5BlockLength, uint64& nextEvent,
    const size_t stillBuffered, Desc& desc) {
  PartialMatch* x = matches->front();
  Paranoid(x != 0 && matches->nextEvent() == off);

  /* Calculate MD5Sum from buf[x->blockOff] to buf[data-1], deal with
     wraparound. NB 0 <= x->blockOff < bufferLength, but 1 <= data <
     bufferLength+1 */
  static MD5Sum md;
  md.reset();
  if (x->blockOffset() < data) {
    md.update(buf + x->blockOffset(), data - x->blockOffset());
  } else {
    md.update(buf + x->blockOffset(), bufferLength - x->blockOffset());
    md.update(buf, data);
  }
  md.finishForReuse();
  //____________________

  const MD5* xfileSum = x->file()->getSums(cache, x->blockNumber());
  if (debug)
    debug("checkMD5Match?: image %1, file %2 block #%3 %4",
          md.toString(), x->file()->leafName(), x->blockNumber(),
          (xfileSum ? xfileSum->toString() : "[error]") );

  if (xfileSum == 0 || md != *xfileSum) {
    /* The block didn't match, so the whole file doesn't match - re-read from
       file any data that is no longer buffered (and not covered by another
       match), and write it to the Zobstream. */
    return checkMD5Match_mismatch(stillBuffered, x, desc);
  }
  //____________________

  // Another block of file matched - was it the last one?
  if (off < x->startOffset() + x->file()->size()) {
    // Still some more to go - update x and its position in queue
    x->setBlockOffset(data);
    x->setBlockNumber(x->blockNumber() + 1);
    x->setNextEvent(matches, min(x->nextEvent() + md5BlockLength,
                                 x->startOffset() + x->file()->size()));
    nextEvent = min(nextEvent, x->nextEvent());
    debug("checkMD5Match: match and more to go, next at off %1",
          x->nextEvent());
    return SUCCESS;
  }
  //____________________

  Assert(off == x->startOffset() + x->file()->size());
  // Heureka! *MATCH*
  // x = address of PartialMatch obj of file that matched

  const PartialMatch* oldestMatch = matches->findLowestStartOffset();

  if (!greedyMatching && x != oldestMatch) {
    // A larger match is possible, so skip this match
    debug("IGNORING match due to --no-greedy-matching: [%1,%2) %3",
          x->startOffset(), off, x->file()->leafName());
    matches->eraseFront(); // return x to free pool
    return SUCCESS;
  }

  reporter.matchFound(x->file(), x->startOffset());
  matchedParts.push_back(x->file());

  /* Re-read and write out data before the start of the match, i.e. of any
     half-finished bigger match (which we abandon now that we've found a
     smaller match inside it). */
  if (x != oldestMatch && oldestMatch->startOffset() < off - stillBuffered) {
    unmatchedStart = min(off - stillBuffered, x->startOffset());
    debugRangeInfo(oldestMatch->startOffset(), unmatchedStart,
                   "UNMATCHED, re-reading partial match from", oldestMatch);
    size_t toReread = unmatchedStart - oldestMatch->startOffset();
    desc.unmatchedData(toReread);
    if (rereadUnmatched(oldestMatch->file(), toReread))
      return FAILURE;
  }

  /* Write out data that is still buffered, and is before the start of the
     match. */
  if (unmatchedStart < x->startOffset()) {
    debugRangeInfo(unmatchedStart, x->startOffset(),
                   "UNMATCHED, buffer flush before match");
    size_t toWrite = x->startOffset() - unmatchedStart;
    Paranoid(off - unmatchedStart <= bufferLength);
    size_t writeStart = modSub(data, off - unmatchedStart, bufferLength);
    writeBuf(buf, writeStart, modAdd(writeStart, toWrite, bufferLength),
             bufferLength, zip);
    desc.unmatchedData(toWrite);
  }

  // Assert(x->file->mdValid);
  desc.matchedFile(x->file()->size(), *(x->file()->getRsyncSum(cache)),
                   *(x->file()->getMD5Sum(cache)));
  unmatchedStart = off;
  debugRangeInfo(x->startOffset(), off, "MATCH:", x);

  // With --match-exec, execute user-supplied command(s)
  if (!matchExec.empty()
      && matchExecCommands(x) == FAILURE)
    return FAILURE;

  /* Remove all matches with startOff < off (this includes x). This is the
     greedy approach and is not ideal: If a small file A happens to match one
     small fraction of a large file B and B is contained in the image, then
     only a match of A will be found. */
  Paranoid(x->startOffset() < off);
  matches->eraseStartOffsetLess(off);

  return SUCCESS;
}
//________________________________________

/* This is called in case the end of the image has been reached, but
   unmatchedStart < off, i.e. there was a partial match at the end of the
   image. Just discards that match and writes the data to the template,
   either by re-reading from the partially matched file, or from the buffer.
   Compare to similar code in checkMD5Match.

   Since we are at the end of the image, the full last bufferLength bytes of
   the image are in the buffer. */
bool MkTemplate::unmatchedAtEnd(byte* const buf,
    const size_t bufferLength, const size_t data, Desc& desc) {
  Paranoid(unmatchedStart < off); // cf. where this is called

  // Re-read and write out data that is no longer buffered.
  const PartialMatch* y = matches->findStartOffset(unmatchedStart);
  if (y != 0 && y->startOffset() < off - bufferLength) {
    unmatchedStart = off - bufferLength;
    debugRangeInfo(y->startOffset(), unmatchedStart,
                   "UNMATCHED at end, re-reading partial match from", y);
    size_t toReread = unmatchedStart - y->startOffset();
    desc.unmatchedData(toReread);
    if (rereadUnmatched(y->file(), toReread))
      return FAILURE;
  }
  // Write out data that is still buffered
  if (unmatchedStart < off) {
    debugRangeInfo(unmatchedStart, off, "UNMATCHED at end");
    size_t toWrite = off - unmatchedStart;
    Assert(toWrite <= bufferLength);
    size_t writeStart = modSub(data, toWrite, bufferLength);
    writeBuf(buf, writeStart, data, bufferLength, zip);
    desc.unmatchedData(toWrite);
    unmatchedStart = off;
  }
  return SUCCESS;
}
//________________________________________

/* The "matches" queue is full. Typically, when this happens there is a big
   zero-filled area in the image and one or more input files start with
   zeroes. At this point, we must start dropping some prospective file
   matches. This is done based on the heuristics that actual file matches
   occur at a "sector boundary" in the image. See INITIAL_SECTOR_LENGTH in
   mktemplate.hh for more.

   This method is a variant of the main loop which is used when
   matches.full(), and which advances the rsum window by one sector at a
   time. */
void MkTemplate::scanImage_mainLoop_fastForward(uint64 nextEvent,
    RsyncSum64* rsum, byte* buf, size_t* data, size_t* n, size_t* rsumBack,
    size_t bufferLength, size_t blockLength, uint32 blockMask,
    size_t md5BlockLength) {

# if 0
  // Simple version
  debug("DROPPING, fast forward (queue full)");
  Assert(off >= blockLength);
  unsigned sectorMask = sectorLength - 1;
  while (off < nextEvent) {
    rsum->removeFront(buf[*rsumBack], blockLength);
    rsum->addBack(buf[*data]);
    ++*data; ++off; --*n;
    *rsumBack = modAdd(*rsumBack, 1, bufferLength);
    if (((off - blockLength) & sectorMask) == 0) {
      checkRsyncSumMatch(*rsum, blockMask, blockLength, *rsumBack,
                         md5BlockLength, nextEvent);
      sectorMask = sectorLength - 1;
      Paranoid(matches->empty()
               || matches->front()->startOffset() >= unmatchedStart);
    }
  }

# else

  debug("DROPPING, fast forward (queue full)");
  Assert(off >= blockLength);

  unsigned sectorMask = sectorLength - 1;
  uint64 notSectorMask = ~implicit_cast<uint64>(sectorMask);
  while (off < nextEvent) {
    /* Calculate next value of off where a match would end up having an even
       (i.e. sectorSize-aligned) start offset.

                             |----sectorLength----|----sectorLength----|
                    |========+========+===========+===+===========+====+=====>EOF
       File offset: 0                             |  off          |
                                      |--blockLength--|           |
                                                  |--blockLength--|
                                                                  |
                                                            nextAlignedOff */
    uint64 nextAlignedOff = off - blockLength;
    nextAlignedOff = (nextAlignedOff + sectorLength) & notSectorMask;
    nextAlignedOff += blockLength;
    Assert(nextAlignedOff > off);

    unsigned len = nextAlignedOff - off;
    if (len > nextEvent - off) len = nextEvent - off;
    // Advance rsum by len bytes in one go
#   if DEBUG
    RsyncSum64 rsum2 = *rsum; size_t rsumBack2 = *rsumBack;
    uint64 off2 = off; size_t data2 = *data;
#   endif
    if (*rsumBack + len <= bufferLength) {
      rsum->removeFront(buf + *rsumBack, len, blockLength);
    } else {
      rsum->removeFront(buf + *rsumBack, bufferLength - *rsumBack,
                        blockLength);
      rsum->removeFront(buf, len + *rsumBack - bufferLength,
                        blockLength + *rsumBack - bufferLength);
    }
    Paranoid(*data + len <= bufferLength);
    rsum->addBack(buf + *data, len);
    *data += len; off += len; *n -= len;
    *rsumBack = modAdd(*rsumBack, implicit_cast<size_t>(len), bufferLength);
    Paranoid(off == nextEvent || off == nextAlignedOff);
#   if DEBUG
    for (unsigned i = 0; i < len; ++i) {
      rsum2.removeFront(buf[rsumBack2], blockLength);
      rsum2.addBack(buf[data2]);
      ++data2; ++off2;
      rsumBack2 = modAdd(rsumBack2, 1, bufferLength);
    }
    Assert(rsumBack2 == *rsumBack);
    Assert(off2 == off);
    Assert(data2 == *data);
    Assert(rsum2 == *rsum);
#   endif

    //debug("DROPPING, fast forward (queue full) to %1", off);

    if (off == nextAlignedOff) {
      Paranoid(((off - blockLength) & sectorMask) == 0);
      checkRsyncSumMatch(*rsum, blockMask, blockLength, *rsumBack,
                         md5BlockLength, nextEvent);
      Paranoid(matches->empty()
               || matches->front()->startOffset() >= unmatchedStart);
      sectorMask = sectorLength - 1;
      notSectorMask = ~implicit_cast<uint64>(sectorMask);
    }
  } // endwhile (off < nextEvent)
# endif
}
//________________________________________

/* Scan image. Central function for template generation.

   Treat buf as a circular buffer. Read new data into at most half the
   buffer. Calculate a rolling checksum covering blockLength bytes. When it
   matches an entry in block, start calculating MD5Sums of blocks of length
   md5BlockLength.

   Since both image and templ can be non-seekable, we run into a problem in
   the following case: After the initial RsyncSum match, a few of the
   md5BlockLength-sized chunks of one input file were matched, but not all,
   so in the end, there is no match. Consequently, we would now need to
   re-read that part of the image and pump it through zlib to templ - but we
   can't if the image is stdin! Solution: Since we know that the MD5Sum of a
   block matched part of an input file, we can re-read from there. */
inline bool MkTemplate::scanImage(byte* buf, size_t bufferLength,
    size_t blockLength, uint32 blockMask, size_t md5BlockLength,
    MD5Sum& templMd5Sum) {
  bool result = SUCCESS;

  /* Cause input files to be analysed */
  if (scanFiles(blockLength, blockMask, md5BlockLength))
    result = FAILURE;

  /* Initialise rolling sums with blockSize bytes 0x7f, and do the same with
     part of buffer, to avoid special-case code in main loop. (Any value
     would do - except that 0x00 or 0xff might lead to a larger number of
     false positives.) */
  RsyncSum64 rsum;
  byte* bufEnd = buf + bufferLength;
  //for (byte* z = bufEnd - blockLength; z < bufEnd; ++z) *z = 0x7f;
  // Init entire buf, keep valgrind happy
  for (byte* z = buf; z < bufEnd; ++z) *z = 0x7f;
  rsum.addBackNtimes(0x7f, blockLength);

  // Compression pipe for templ data
  auto_ptr<Zobstream> zipDel;
  if (useBzLib)
    zipDel.reset(implicit_cast<Zobstream*>(
      new ZobstreamBz(*templ, zipQual, 256U, &templMd5Sum) ));
  else
    zipDel.reset(implicit_cast<Zobstream*>(
      new ZobstreamGz(*templ, ZIPCHUNK_SIZE, zipQual, 15, 8, 256U,
                      &templMd5Sum) ));
  zip = zipDel.get();
  Desc desc; // Buffer for DESC data, will be appended to templ at end
  size_t data = 0; // Offset into buf of byte currently being processed
  off = 0; // Current absolute offset in image, corresponds to "data"
  uint64 nextReport = 0; // call reporter once off reaches this value

  /* The area delimited by unmatchedStart (incl) and off (excl) has "not been
     dealt with", either by writing it to zip, or by a match with the first
     MD5 block of an input file. Once a partial match of a file has been
     detected, unmatchedStart "gets stuck" at the start offset of this file
     within the image. */
  unmatchedStart = 0;

  MD5Sum imageMd5Sum; // MD5 of whole image
  MD5Sum md; // Re-used for each 2nd-level check of any rsum match
  matches->erase();
  sectorLength = INITIAL_SECTOR_LENGTH;

  // Read image
  size_t rsumBack = bufferLength - blockLength;

  try {
    /* Catch Zerrors, which can occur in zip->write(), writeBuf(),
       checkMD5Match(), zip->close() */
    while (image->good()) {

      debug("---------- main loop. off=%1 data=%2 unmatchedStart=%3",
            off, data, unmatchedStart);

      if (off >= nextReport) { // Keep user entertained
        reporter.scanningImage(off);
        nextReport += REPORT_INTERVAL;
      }

      // zip->write() out any old data that we'll destroy with read()
      size_t thisReadAmount = (readAmount < bufferLength - data ?
                               readAmount : bufferLength - data);
      uint64 newUnmatchedStart = 0;
      if (off > blockLength)
        newUnmatchedStart = off - blockLength;
      if (!matches->empty()) {
        newUnmatchedStart = min(newUnmatchedStart,
                                matches->lowestStartOffset()->startOffset());
      }
      if (unmatchedStart < newUnmatchedStart) {
        size_t toWrite = newUnmatchedStart - unmatchedStart;
        Paranoid(off - unmatchedStart <= bufferLength);
        //debug("off=%1 unmatchedStart=%2 buflen=%3",
        //      off, unmatchedStart, bufferLength);
        size_t writeStart = modSub(data, off - unmatchedStart,
                                   bufferLength);
        debugRangeInfo(unmatchedStart, unmatchedStart + toWrite,
                       "UNMATCHED");
        writeBuf(buf, writeStart,
                 modAdd(writeStart, toWrite, bufferLength),
                 bufferLength, zip);
        unmatchedStart = newUnmatchedStart;
        desc.unmatchedData(toWrite);
      }

      // Read new data from image
#     if DEBUG // just for testing, make it sometimes read less
      static size_t chaosOff, acc;
      if (unmatchedStart == 0) { chaosOff = 0; acc = 0; }
      acc += buf[chaosOff] ^ buf[off % bufferLength] + ~off;
      if (chaosOff == 0) chaosOff = bufferLength - 1; else --chaosOff;
      if ((acc & 0x7) == 0) {
        thisReadAmount = acc % thisReadAmount + 1;
        debug("thisReadAmount=%1", thisReadAmount);
      }
#     endif
      readBytes(*image, buf + data, thisReadAmount);
      size_t n = image->gcount();
      imageMd5Sum.update(buf + data, n);

      while (n > 0) { // Still unprocessed bytes left
        uint64 nextEvent = off + n; // Special event: end of buffer
        if (!matches->empty())
          nextEvent = min(nextEvent, matches->front()->nextEvent());

        if (!matches->full()) {
          sectorLength = INITIAL_SECTOR_LENGTH;

          /* Unrolled innermost loop - see below for single-iteration
             version. Also see checkRsyncSumMatch above. */
          while (off + 32 < nextEvent && rsumBack < bufferLength - 32) {
            size_t dataOld = data;
            do {
              const vector<FilePart*>* hashEntry;
#             define x ; \
              rsum.removeFront(buf[rsumBack], blockLength); \
              rsum.addBack(buf[data]); \
              ++data; ++rsumBack; \
              hashEntry = &block[rsum.getHi() & blockMask]; \
              if (hashEntry->size() > 1) break; \
              if (hashEntry->size() == 1) { \
                FilePart* file = (*hashEntry)[0]; \
                const RsyncSum64* fileSum = file->getRsyncSum(cache); \
                if (fileSum != 0 && *fileSum == rsum) break; \
              }
              x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x x
#             undef x
              dataOld = data; // Special case: "Did not break out of loop"
            } while (false);
            if (dataOld == data) {
              off += 32; n -= 32;
            } else {
              dataOld = data - dataOld; off += dataOld; n -= dataOld;
              checkRsyncSumMatch(rsum, blockMask, blockLength, rsumBack,
                                 md5BlockLength, nextEvent);
            }
            if (matches->full()) break;
          }
        } // endif (!matches->full())

        if (!matches->full()) {
          // Innermost loop - single-byte version, matches not full
          while (off < nextEvent) {
            // Roll checksum by one byte
            rsum.removeFront(buf[rsumBack], blockLength);
            rsum.addBack(buf[data]);
            ++data; ++off; --n;
            rsumBack = modAdd(rsumBack, 1, bufferLength);

            /* Look for matches of rsum. If found, insert appropriate
               entry in matches list and maybe modify nextEvent. */
            checkRsyncSumMatch(rsum, blockMask, blockLength, rsumBack,
                               md5BlockLength, nextEvent);

            /* We mustn't by accident schedule an event for a part of
               the image that has already been flushed out of the
               buffer/matched */
            Paranoid(matches->empty()
                     || matches->front()->startOffset() >= unmatchedStart);
          }
        } else {
          // Innermost loop - MATCHES IS FULL
          scanImage_mainLoop_fastForward(nextEvent, &rsum, buf, &data, &n,
              &rsumBack, bufferLength, blockLength, blockMask,
              md5BlockLength);
        } // endif (matches->full())
        if (matches->empty())
          debug(" %1: Event, matches empty", off);
        else
          debug(" %1: Event, matchesOff=%2", off, matches->nextEvent());

        /* Calculate MD5 for the previous md5ChunkLength (or less if
           at end of match) bytes, if necessary. If the calculated
           checksum matches and it is the last MD5 block in the file,
           record a file match. If the i-th MD5Sum does not match,
           write the i*md5ChunkLength bytes directly to templ. */
        while (!matches->empty() && matches->nextEvent() == off) {
          size_t stillBuffered = bufferLength - n;
          if (stillBuffered > off) stillBuffered = off;
          if (checkMD5Match(buf, bufferLength, data, md5BlockLength,
                            nextEvent, stillBuffered, desc))
            return FAILURE; // no recovery possible, exit immediately
        }

        Assert(matches->empty() || matches->nextEvent() > off);
      } // endwhile (n > 0), i.e. more unprocessed bytes left in buffer

      if (data == bufferLength) data = 0;
      Assert(data < bufferLength);

    } // endwhile (image->good()), i.e. more data left in input image

    // End of image data - any remaining partial match is UNMATCHED
    if (unmatchedStart < off
        && unmatchedAtEnd(buf, bufferLength, data, desc)) {
      return FAILURE;
    }
    Assert(unmatchedStart == off);

    zip->close();
  }
  catch (Zerror ze) {
    string err = subst(_("Error during compression: %1"), ze.message);
    reporter.error(err);
    try { zip->close(); } catch (Zerror zze) { }
    return FAILURE;
  }

  imageMd5Sum.finish();
  desc.imageInfo(off, imageMd5Sum, cache->getBlockLen());
  desc.put(*templ, &templMd5Sum);
  if (!*templ) {
    string err = _("Could not write template data");
    reporter.error(err);
    return FAILURE;
  }

  reporter.finished(off);
  return result;
}
//______________________________________________________________________

// Central function which processes the data and finds matches
bool MkTemplate::run(const string& imageLeafName,
                     const string& templLeafName) {
  bool result = SUCCESS;
  oldAreaEnd = 0;

  // Kick out files that are too small
  for (JigdoCache::iterator f = cache->begin(), e = cache->end();
       f != e; ++f) {
    if (f->size() < cache->getBlockLen()) {
      f->markAsDeleted(cache);
      continue;
    }
    fileSizeTotal += f->size();
    ++fileCount;
  }

  // Hash table performance drops when linked lists become long => "+1"
  int    blockBits = bitWidth(fileCount) + 1;
  uint32 blockMask = (1 << blockBits) - 1;
  block.resize(blockMask + 1);

  size_t max_MD5Len_blockLen =
      cache->getBlockLen() + 64; // +64 for Assert below
  if (max_MD5Len_blockLen < cache->getMD5BlockLen())
    max_MD5Len_blockLen = cache->getMD5BlockLen();
  /* Pass 1 imposes no minimum buffer length, only pass 2: Must always
     be able to read readAmount bytes into one buffer half in one go;
     must be able to start calculating an MD5Sum at a position that is
     blockLength bytes back in input; must be able to write out at
     least previous md5BlockLength bytes in case there is no match. */
  size_t bufferLength = 2 *
    (max_MD5Len_blockLen > readAmount ? max_MD5Len_blockLen : readAmount);
  // Avoid reading less bytes than readAmount at any time
  bufferLength = (bufferLength + readAmount - 1) / readAmount * readAmount;

  Paranoid(bufferLength % readAmount == 0); // for efficiency only
  // Asserting this makes things easier in pass 2. Yes it is ">" not ">="
  Assert(cache->getMD5BlockLen() > cache->getBlockLen());

  if (debug) {
    debug("Nr of files: %1 (%2 bits)", fileCount, blockBits);
    debug("Total bytes: %1", fileSizeTotal);
    debug("blockLength: %1", cache->getBlockLen());
    debug("md5BlockLen: %1", cache->getMD5BlockLen());
    debug("bufLen (kB): %1", bufferLength/1024);
    debug("zipQual:     %1", zipQual);
  }

  MD5Sum templMd5Sum;
  ArrayAutoPtr<byte> bufDel(new byte[bufferLength]);
  byte* buf = bufDel.get();

  prepareJigdo(); // Add [Jigdo]

  { // Write header to template file
    string s = TEMPLATE_HDR; append(s, FILEFORMAT_MAJOR); s += '.';
    append(s, FILEFORMAT_MINOR); s += " jigdo-file/" JIGDO_VERSION;
    s += "\r\nSee "; s += URL; s += " for details about jigdo.\r\n\r\n";
    const byte* t = reinterpret_cast<const byte*>(s.data());
    writeBytes(*templ, t, s.size());
    templMd5Sum.update(t, s.size());
    if (!*templ) result = FAILURE;
  }

  // Read input image and output parts that do not match
  if (scanImage(buf, bufferLength, cache->getBlockLen(), blockMask,
                cache->getMD5BlockLen(), templMd5Sum)) {
    result = FAILURE;
  }
  cache->deallocBuffer();
  templMd5Sum.finish();

  // Add [Image], (re-)add [Parts]
  finalizeJigdo(imageLeafName, templLeafName, templMd5Sum);

  debug("MkTemplate::run() finished");
  return result;
}

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Scanning of input files

*/

#include <config.h>

#include <iostream>
#include <fstream>
#include <string>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <bstream.hh>
#include <compat.hh>
#include <configfile.hh>
#include <log.hh>
#include <scan.hh>
#include <string.hh>
#include <serialize.hh>
//______________________________________________________________________

DEBUG_UNIT("scan")

void JigdoCache::ProgressReporter::error(const string& message) {
  cerr << message << endl;
}
void JigdoCache::ProgressReporter::info(const string& message) {
  cerr << message << endl;
}
void JigdoCache::ProgressReporter::scanningFile(const FilePart*, uint64) { }

JigdoCache::ProgressReporter JigdoCache::noReport;

struct stat JigdoCache::fileInfo;
//______________________________________________________________________

#if HAVE_LIBDB
/* Interpret a string of bytes (out of the file cache) like this:

   4   blockLength (of rsync sum)
   4   md5BlockLength
   4   blocks (number of valid md5 blocks in this entry), curr. always >0
   8   rsyncSum of file start (only valid if blocks > 0)
  16   fileMD5Sum (only valid if
                   blocks == (fileSize+md5BlockLength-1)/md5BlockLength )
  followed by n entries:
  16   md5sum of block of size md5BlockLength

  If stored md5BlockLength doesn't match supplied length, do nothing.
  Otherwise, restore *this from cached data and return cached
  blockLength (0 if not cached). The caller needs to make sure the
  blockLength matches.

  This is not a standard unserialize() member of FilePart because it
  does not create a complete serialization - e.g. the location path
  iter is missing. It only creates a cache entry. */

size_t FilePart::unserializeCacheEntry(const byte* data, size_t dataSize,
                                       size_t md5BlockLength){
  Assert(dataSize > PART_MD5SUM);

  // The resize() must have been made by the caller
  Paranoid(sums.size() == (size() + md5BlockLength - 1) / md5BlockLength);

  size_t cachedBlockLength;
  data = unserialize4(cachedBlockLength, data);
  size_t cachedMd5BlockLength;
  data = unserialize4(cachedMd5BlockLength, data);
  if (cachedMd5BlockLength != md5BlockLength) return 0;

  size_t blocks;
  data = unserialize4(blocks, data);
  // Ignore strange-looking entries
  if (blocks * serialSizeOf(md5Sum) != dataSize - PART_MD5SUM
      || blocks == 0) {
    if (blocks == 0) debug("ERR #blocks == 0");
    else debug("ERR wrong entry size (%1 vs %2)",
               blocks * 16, dataSize - PART_MD5SUM);
    return 0;
  }
  Paranoid(serialSizeOf(rsyncSum) == 8);
  data = unserialize(rsyncSum, data);
  Paranoid(serialSizeOf(md5Sum) == 16);
  // All blocks of file present?
  if (blocks == sums.size()) {
    setFlag(MD_VALID);
    data = unserialize(md5Sum, data);
  } else {
    clearFlag(MD_VALID);
    data += 16;
  }
  // Read md5 sums of individual chunks of file
  vector<MD5>::iterator sum = sums.begin();
  for (size_t i = blocks; i > 0; --i) {
    data = unserialize(*sum, data);
    ++sum;
  }

  return cachedBlockLength;
}
#endif
//______________________________________________________________________

#if HAVE_LIBDB
/** Opposite of unserializeCacheEntry; create byte stream from object */
struct FilePart::SerializeCacheEntry {
  SerializeCacheEntry(const FilePart& f, JigdoCache* c, size_t blockLen,
                      size_t md5Len)
    : file(f), cache(c), blockLength(blockLen), md5BlockLength(md5Len) { }
  const FilePart& file;
  JigdoCache* cache;
  size_t blockLength;
  size_t md5BlockLength;

  size_t serialSizeOf() {
    return PART_MD5SUM + (file.mdValid() ? file.sums.size() * 16 : 16);
  }

  void operator()(byte* data) {
    Paranoid(file.getFlag(TO_BE_WRITTEN));
    // If empty(), shouldn't have been marked TO_BE_WRITTEN:
    Assert(!file.sums.empty());

    data = serialize4(blockLength, data);
    data = serialize4(md5BlockLength, data);
    // Nr of valid blocks - either 1 or all
    size_t blocks = (file.mdValid() ? file.sums.size() : 1);
    data = serialize4(blocks, data);
    data = serialize(file.rsyncSum, data);
    data = serialize(file.md5Sum, data);
    // Write md5 sums of individual chunks of file
    vector<MD5>::const_iterator sum = file.sums.begin();
    for (size_t i = blocks; i > 0; --i) {
      data = serialize(*sum, data);
      ++sum;
    }
  }
};
#endif
//______________________________________________________________________

#if HAVE_LIBDB
JigdoCache::JigdoCache(const string& cacheFileName, size_t expiryInSeconds,
                       size_t bufLen, ProgressReporter& pr)
  : blockLength(0), md5BlockLength(0), files(), nrOfFiles(0),
    locationPaths(), readAmount(bufLen), buffer(), reporter(pr),
    cacheExpiry(expiryInSeconds) {
  cacheFile = 0;
  try {
    if (!cacheFileName.empty())
      cacheFile = new CacheFile(cacheFileName.c_str());
  } catch (DbError e) {
    string err = subst(_("Could not open cache file: %L1"), e.message);
    reporter.error(err);
  }
}
#else
JigdoCache::JigdoCache(const string&, size_t, size_t bufLen,
                       ProgressReporter& pr)
  : blockLength(0), md5BlockLength(0), files(), nrOfFiles(0),
    locationPaths(), readAmount(bufLen), buffer(), reporter(pr) { }
#endif
//______________________________________________________________________

JigdoCache::~JigdoCache() {
# if HAVE_LIBDB
  if (cacheFile) {
    // Write out any cache entries that need it
    for (list<FilePart>::const_iterator i = files.begin(), e = files.end();
         i != e; ++i) {
      if (i->deleted() || !i->getFlag(FilePart::TO_BE_WRITTEN)) continue;
      debug("Writing %1", i->leafName());
      FilePart::SerializeCacheEntry serializer(*i, this, blockLength,
                                               md5BlockLength);
      try {
        cacheFile->insert(serializer, serializer.serialSizeOf(),
                          i->leafName(), i->mtime(), i->size());
      } catch (DbError e) {
        reporter.error(e.message);
      }
    }

    if (cacheExpiry > 0) {
      // Expire old cache entries from cache
      time_t expired = time(0);
      Paranoid(expired != static_cast<time_t>(-1));
      expired -= cacheExpiry;
      try {
        cacheFile->expire(expired);
      } catch (DbError e) {
        string err = subst(_("Error during cache expiry: %1. The cache "
                             "file may be corrupt, consider deleting it."),
                           e.message);
        reporter.error(err);
      }
    }

    // Close db object, flushing changes to disc
    delete cacheFile;
  }
# endif
}
//______________________________________________________________________

/* Either reads data for the first MD5 block and creates sums[0] and
   rsyncSum, or reads whole file and creates all sums[] entries and
   rsyncSum and the whole file's MD5 sum. */
const MD5* FilePart::getSumsRead(JigdoCache* c, size_t blockNr) {
  // Should do this check before calling:
  Paranoid((blockNr == 0 && sums.empty()) || !mdValid());

  // Do not forget to setParams() before calling this!
  Assert(c->md5BlockLength != 0);
  const size_t thisBlockLength = c->blockLength;

  sums.resize((size() + c->md5BlockLength - 1) / c->md5BlockLength);
  //____________________

# if HAVE_LIBDB
  // Can we maybe get the info from the cache?
  if (c->cacheFile != 0 && !getFlag(WAS_LOOKED_UP)) {
    setFlag(WAS_LOOKED_UP);
    const byte* data;
    size_t dataSize;
    try {
      /* Unserialize will do nothing if md5BlockLength differs. If
         md5BlockLength matches, but returned blockLength doesn't, we
         need to re-read the first block. */
      if (c->cacheFile->find(data, dataSize, leafName(), size(), mtime())) {
        debug("%1 found, want block#%2", leafName(), blockNr);
        size_t cachedBlockLength = unserializeCacheEntry(data, dataSize,
                                                         c->md5BlockLength);
        // Was all necessary data in cache? Yes => return it now.
        if (cachedBlockLength == thisBlockLength
            && (blockNr == 0 || mdValid())) {
          debug("%1 loaded, blockLen (%2) matched, %3/%4 in cache",
                leafName(), thisBlockLength, (mdValid() ? sums.size() : 1),
                sums.size());
          return &sums[blockNr];
        }
        /* blockLengths didn't match and/or the cache only contained
           the first md5 sum while we asked for a later one. It's as
           if we never queried the cache, except for the case when we
           need to re-read the first block because the blockLength
           changed, but *all* blocks' md5sums were in the cache. */
        debug("%1 loaded, NO match (blockLen %2 vs %3), %4/%5 in cache",
              leafName(), cachedBlockLength, thisBlockLength,
              (mdValid() ? sums.size() : 1), sums.size());
      }
    } catch (DbError e) {
      string err = subst(_("Error accessing cache: %1"), e.message);
      c->reporter.error(err);
    }
  }
# endif /* HAVE_LIBDB */
  //____________________

  // Open input file
  string name(getPath());
  name += leafName();
  bifstream input(name.c_str(), ios::binary);
  if (!input) {
    string err;
    if (name == "-") {
      /* Actually, stdin /would/ be allowed /here/, but it isn't
         possible with mktemplate. */
      err = _("Error opening file `-' "
              "(using standard input not allowed here)");
    } else {
      err = subst(_("Could not open `%L1' for input - excluded"), name);
      if (errno != 0) {
        err += " (";
        err += strerror(errno);
        err += ')';
      }
    }
    markAsDeleted(c);
    c->reporter.error(err); // might throw
    return 0;
  }
  //____________________

  // We're going to write this to the cache later on
  setFlag(TO_BE_WRITTEN);

  // Allocate or resize buffer, or do nothing if already right size
  c->buffer.resize(c->readAmount > c->md5BlockLength ?
                   c->readAmount : c->md5BlockLength);
  //______________________________

  // Read data and create sums

  uint64 off = 0; // File offset of first byte in buf
  // Nr of bytes before we are to reset() md
  size_t mdLeft = c->md5BlockLength;
  /* Call reporter once off reaches this value - only report something
     if scanning >1 md5 block */
  uint64 nextReport = mdLeft;
  MD5Sum md;
  md5Sum.reset();
  vector<MD5>::iterator sum = sums.begin();
  //____________________

  // Calculate RsyncSum of head of file and MD5Sums for all blocks

  Assert(thisBlockLength <= c->md5BlockLength);
  byte* buf = &c->buffer[0];
  byte* bufpos = buf;
  byte* bufend = buf + (c->readAmount > thisBlockLength ?
                        c->readAmount : thisBlockLength);
  while (input && static_cast<size_t>(bufpos - buf) < thisBlockLength) {
    readBytes(input, bufpos, bufend - bufpos);
    size_t nn = input.gcount();
    bufpos += nn;
    debug("Read %1", nn);
  }
  size_t n = bufpos - buf;
  // Create RsyncSum of 1st bytes of file, or leave at 0 if file too small
  rsyncSum.reset();
  if (n >= thisBlockLength) rsyncSum.addBack(buf, thisBlockLength);
  //__________

  while (true) { // Will break out if error or whole file read

    // n is number of valid bytes in buf[]
    off += n;
    if (off > size()) break; // Argh - file size changed

    if (off >= nextReport) {
      c->reporter.scanningFile(this, off);
      nextReport += REPORT_INTERVAL;
    }

    // Create MD5 for chunks of size md5BlockLength
    if (n < mdLeft) {
      md.update(buf, n);
      mdLeft -= n;
    } else {
      md.update(buf, mdLeft);
      byte* cur = buf + mdLeft;
      size_t nn = n - mdLeft;
      do {
        md.finishForReuse();
        debug("%1: mdLeft (0), switching to next md at off %2, left %3, "
              "writing sum#%4: %5", name, off - n + cur - buf, nn,
              sum - sums.begin(), md.toString());
        Paranoid(sum != sums.end());
        *sum = md;
        ++sum;
        size_t m = (nn < c->md5BlockLength ? nn : c->md5BlockLength);
        md.reset().update(cur, m);
        cur += m; nn -= m;
        mdLeft = c->md5BlockLength - m;
      } while (nn > 0);
    }

    md5Sum.update(buf, n); // Create MD5 for the whole file

    if (blockNr == 0 && sum != sums.begin()) break; // Only wanted 1st block
    if (!input) break; // End of file or error

    // Read more data
    readBytes(input, buf, c->readAmount);
    n = input.gcount();
    debug("%1: read %2", name, n);

  } // Endwhile (true), will break out if error or whole file read

  Paranoid(sum != sums.end() // >=1 trailing bytes
           || mdLeft == c->md5BlockLength); // 0 trailing bytes
  if (off == size() && input.eof()) {
    // Whole file was read
    c->reporter.scanningFile(this, size()); // 100% scanned
    if (mdLeft < c->md5BlockLength) {
      (*sum) = md.finish(); // Digest of trailing bytes
      debug("%1: writing trailing sum#%2: %3",
            name, sum - sums.begin(), md.toString());
    }
    md5Sum.finish(); // Digest of whole file
    setFlag(MD_VALID);
    return &sums[blockNr];
  } else if (blockNr == 0 && sum != sums.begin()) {
    // Only first md5 block of file was read
    debug("%1: file header read, sum#0 written", name);
#   if DEBUG
    md5Sum.finish(); // else failed assert in FilePart::SerializeCacheEntry
#   else
    md5Sum.abort(); // Saves the memory until whole file is read
#   endif
    return &sums[0];
  }
  //____________________

  // Some error happened
  string err = subst(_("Error while reading `%1' - file will be ignored "
                       "(%2)"), name, strerror(errno));
  markAsDeleted(c);
  c->reporter.error(err);
  return 0;
}
//______________________________________________________________________

const MD5Sum* FilePart::getMD5SumRead(JigdoCache* c) {
  if (getSumsRead(c,
                  (fileSize + c->md5BlockLength - 1) / c->md5BlockLength - 1)
      == 0) return 0;
  Paranoid(mdValid());
  return &md5Sum;
}
//______________________________________________________________________

void JigdoCache::setParams(size_t blockLen, size_t md5BlockLen) {
  if (blockLen == blockLength && md5BlockLen == md5BlockLength) return;

  blockLength = blockLen;
  md5BlockLength = md5BlockLen;
  Assert(blockLength <= md5BlockLength);
  for (list<FilePart>::iterator file = files.begin(), end = files.end();
       file != end; ++file) {
    file->sums.resize(0);
  }
}
//______________________________________________________________________

void JigdoCache::addFile(const string& name) {
  // Do not forget to setParams() before calling this!
  Assert(md5BlockLength != 0);
  // Assumes nonempty filenames
  Paranoid(name.length() > 0);

  // Find "//" in path and if present split name there
  string::size_type pathLen = name.rfind(SPLITSEP);
  string fileUri("file:");
  string path, nameRest;
  if (pathLen == string::npos) {
    size_t splitAfter = 0;
#   if WINDOWS
    // Split after "\" or ".\" or "C:" or "C:\" or "C:.\"
    if (name.length() > 1 && isalpha(name[0]) && name[1] == ':')
      splitAfter = 2;
    if (name.length() > splitAfter && name[splitAfter] == '\\') {
      // If an absolute path, split after leading '\'
      ++splitAfter;
    } else if (name.length() > splitAfter + 1
               && name[splitAfter] == '.' && name[splitAfter + 1] == '\\') {
      // Otherwise, also split after ".\" at start
      splitAfter += 2;
    }
#   else
    // If an absolute path, split after leading '/'
    if (name[0] == DIRSEP) splitAfter = 1;
#   endif
    path.assign(name, 0, splitAfter);
    fileUri += path;
    nameRest.assign(name, splitAfter, string::npos);
  } else {
    // e.g. for name = "dir//file"
    path.assign(name, 0, pathLen + 1); // path = "dir/"
    fileUri.append(name, 0, pathLen + 1); // fileUri = "file:dir/"
    // nameRest = "file"
    nameRest.assign(name, pathLen + sizeof(SPLITSEP) - 1, string::npos);
  }
  compat_swapFileUriChars(fileUri); // Directory separator is always '/'
  ConfigFile::quote(fileUri);
  //____________________

  // If necessary, create a label for the path before "//"
  static string emptylabel;
  LocationPath tmp(path, emptylabel, fileUri);
  LocationPathSet::iterator i = locationPaths.find(tmp);
  if (i == locationPaths.end())
    i = locationPaths.insert(tmp).first; // Any new entry has a "" label
  Paranoid(i != locationPaths.end());

  // Append new obj at end of list
  FilePart fp(i, nameRest, fileInfo.st_size, fileInfo.st_mtime);
  files.push_back(fp);
}

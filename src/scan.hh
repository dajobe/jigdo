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

#ifndef SCAN_HH
#define SCAN_HH

#include <config.h>

#include <list>
#include <set>
#include <vector>
#include <time.h>
#include <sys/stat.h>

#include <cachefile.hh>
#include <debug.hh>
#include <md5sum.hh>
#include <recursedir.fh>
#include <rsyncsum.hh>
#include <scan.fh>
//______________________________________________________________________

/** First part of the filename of a "part", a directory on the local
    filesystem. It will not appear in the location list - it is just
    prepended to the filename to actually access the file data. */
class LocationPath {
  /// Objects are only created by JigdoCache
  friend class JigdoCache;
public:
  // Sort LocationPaths by the directory name
  bool operator<(const LocationPath& x) const { return path < x.path; }
  const string& getPath() const { return path; }
  const string& getLabel() const { return label; }
  LocationPath& setLabel(const string& l) { label = l; return *this; }
  const string& getUri() const { return uri; }
private:
  template <class StringP, class StringL, class StringU>
  inline LocationPath(const StringP& p, const StringL& l,
                      const StringU& u = "");
  // Using default dtor & copy ctor
  // The data members are accessed directly by JigdoCache methods
  string path; // name of directory
  string label; // short label to identify path in location list
  string uri; // e.g. URL for directory on FTP server
};

typedef set<LocationPath> LocationPathSet;
//______________________________________________________________________

/** One of the "parts" (=single files) that the image (=big file)
    consists of. The object is also responsible for scanning the file
    and creating a) an RsyncSum64 of the first blockLength bytes (as
    determined by JigdoCache). Caching is done in a "lazy" manner;
    first, only the file size and name is read, then the MD5 sum of
    the file's first block as well as the Rsync sum is calculated, and
    only when the 2nd MD5 sum or the entire file's sum is requested,
    the file is scanned til EOF. */
class FilePart {
  /// Objects are only created by JigdoCache
  friend class JigdoCache;
public:
  /** Sort FileParts by RsyncSum of first bytes */
  inline const string& getPath() const;
  LocationPathSet::iterator getLocation() { return path; }
  /** @return The further dir names and the leafname, after what getPath()
      returns. */
  inline const string& leafName() const;
  inline uint64 size() const;
  inline time_t mtime() const;
  /** Returns null ptr if error and you don't throw it in your
      JigdoCache error handler */
  inline const MD5* getSums(JigdoCache* c, size_t blockNr);
  /// Returns null ptr if error and you don't throw it
  inline const MD5Sum* getMD5Sum(JigdoCache* c);
  /// Returns null ptr if error and you don't throw it
  inline const RsyncSum64* getRsyncSum(JigdoCache* c);

  /** Mark the FilePart as deleted. Unlike the STL containers'
      erase(), this means that any iterator pointing to the element
      stays valid, you just cannot access the element.
      TODO: Check whether this feature is really needed anywhere
      anymore - I suspect not, maybe we should just delete the
      element. */
  inline void markAsDeleted(JigdoCache* c);
  /** True if this file will be ignored (but not destroyed with
      "delete") by JigdoCache because attempts to open/read it
      failed, or other reasons. */
  bool deleted() const { return fileSize == 0; }

  /** Do not call - this is public only because list<> must be able to
      delete FileParts */
  ~FilePart() { }
  // default copy ctor / assignment op
  //__________

private:
  inline FilePart(LocationPathSet::iterator p, string rest, uint64 fSize,
                  time_t fMtime);

  /* Called when the methods getSums/getMD5Sum() need data read from
     file. Might return null. */
  const MD5* getSumsRead(JigdoCache* c, size_t blockNr);
  const MD5Sum* getMD5SumRead(JigdoCache* c);
  //__________

  /* There are 3 states of a FilePart:
     a) sums.empty(): File has not been read from so far
     b) !sums.empty() && !mdValid: sums[0] and rsyncSum are valid
     c) !sums.empty() && mdValid: all sums[] and rsyncSum and md5Sum valid*/

  LocationPathSet::iterator path;
  string pathRest; // further dir names after "path", and leafname of file
  uint64 fileSize;
  time_t fileMtime;

  /* RsyncSum64 of the first MkTemplate::blockLength bytes of the
     file. */
  RsyncSum64 rsyncSum;
  bool rsyncValid() const { return sums.size() > 0; }

  /* File is split up into chunks of length md5BlockLength (the last
     one may be smaller) and the MD5 checksum of each is
     calculated. */
  vector<MD5> sums;

  /* Hash of complete file contents. mdValid is true iff md5Sum is
     cached and valid. NB, it is possible that mdValid==true, but
     sums.size()==0, after md5BlockSize has been changed. If
     sums.size()==fileSize/md5ChunkSize, then not necessarily
     mdValid==true */
  MD5Sum md5Sum;
  enum Flags {
    EMPTY = 0,
    // Bit flag is set iff md5Sum contains the whole file's md5 sum
    MD_VALID = 1,
    /* This file was looked up in the cache file (whether successfully
       or not doesn't matter) - don't look it up again. */
    WAS_LOOKED_UP = 2,
    // Write this file's info into the cache file during ~JigdoCache()
    TO_BE_WRITTEN = 4
  };
  Flags flags;
  bool getFlag(Flags f) const { return (flags & f) != 0; }
  inline void setFlag(Flags f);
  inline void clearFlag(Flags f);
  bool mdValid() const { return getFlag(MD_VALID); }

# if HAVE_LIBDB
  // Offsets for binary representation in database (see cachefile.hh)
  enum {
    BLOCKLEN = 0, MD5BLOCKLEN = 4, MD5BLOCKS = 8, RSYNCSUM = 12,
    FILE_MD5SUM = 20, PART_MD5SUM = 36
  };

  size_t unserializeCacheEntry(const byte* data, size_t dataSize,
      size_t md5BlockLength); // Byte stream => FilePart
  struct SerializeCacheEntry; // FilePart => byte stream
  friend struct SerializeCacheEntry;
# endif
};
//______________________________________________________________________

/** If a JigdoCacheError happens during any call to a FilePart or
    JigdoCache method, repeating that call is possible to continue. */
struct JigdoCacheError : Error {
  explicit JigdoCacheError(const string& m) : Error(m) { }
  explicit JigdoCacheError(const char* m) : Error(m) { }
};

/** A list of FileParts that is "lazy": Nothing is actually read from
    the files passed to it until that is really necessary. JigdoCache
    behaves like a list<FilePart>.

    A JigdoCache cannot hold zero-length files. They will be silently
    skipped when you try to add them.

    Important note: If you decide to throw the errors handed to your
    errorHandler, JigdoCacheErrors can be thrown by some methods. The
    default cerrPrint (guess what) only outputs the error message to
    cerr. */
class JigdoCache {
  friend class FilePart;
public:
  class ProgressReporter;
  /// cacheFileName can be "" for no file cache
  explicit JigdoCache(const string& cacheFileName,
      size_t expiryInSeconds = 60*60*24*30, size_t bufLen = 128*1024,
      ProgressReporter& pr = noReport);
  /// The dtor will try to write cached data to the cache file
  ~JigdoCache();

  /** Read a list of filenames from the object and store them in the
      JigdoCache. Only the file size is read during the call, Rsync/MD5
      sums are calculated later, if/when needed. */
  template <class RecurseDir>
  inline void readFilenames(RecurseDir& rd);
  /** Set the sizes of cache's blockLength and md5BlockLength
      parameters. This means that the checksum returned by a
      FilePart's getRsyncSum() method will cover the specified size.
      NB: If the cache already contains files and the new parameters
      are not the same as the old, the start of the files will
      (eventually) have to be re-read to re-calculate the checksums
      for blockLength, and the *whole* file will have to be re-read
      for a changed md5BlockLength. */
  void setParams(size_t blockLen,size_t md5BlockLen);
  size_t getBlockLen() const { return blockLength; }
  size_t getMD5BlockLen() const { return md5BlockLength; }

  /** Amount of data that JigdoCache will attempt to read per call to
      ifstream::read(), and size of buffer allocated. Minimum: 64k */
  inline void setReadAmount(size_t bytes);
  /** To speed things up, JigdoCache by default keeps one file open and
      a read buffer allocated. You can close the file and deallocate
      the buffer with a call to this method. They will be
      re-opened/re-allocated automatically if/when needed. */
  void deallocBuffer() { buffer.resize(0); }

  /// Return reporter supplied by JigdoCache creator
  ProgressReporter* getReporter() { return &reporter; }

  /// Returns number of files in cache
  inline size_t size() const { return nrOfFiles; }

  /** Make a label for a certain directory name known. E.g.
      <tt>addLabel("pub/debian/","deb")</tt> will cause the file
      <tt>pub/debian&#2f;/dists/stable/x.deb</tt> to appear as
      <tt>deb:dists/stable/x.deb</tt> in the location list. Each of
      the paramters can bei either <tt>string</tt> or <tt>const
      char*</tt>.
      Note: If no labels are defined, a name will automatically be
      created later: dirA, dirB, ..., dirZ, dirAA, ... */
  template <class StringP, class StringL, class StringU>
  const LocationPathSet::iterator addLabel(
    const StringP& path, const StringL& label, const StringU& uri = "");

  /// Access to the members like for a list<>
  typedef FilePart value_type;
  typedef FilePart& reference;
  //____________________

  /// Only part of the iterator functionality implemented ATM
  class iterator {
    friend class JigdoCache;
    friend class FilePart;
  public:
    iterator() { }
    inline iterator& operator++(); // might throw(RecurseError, bad_alloc)
    FilePart& operator*() { return *part; }
    FilePart* operator->() { return &*part; }
    // Won't compare cache members - their being different is usu. a bug
    bool operator==(const iterator& i) const {
      Paranoid(cache == i.cache);
      return part == i.part;
    }
    bool operator!=(const iterator& i) const { return !(*this == i); }
    // Default dtor
  private:
    iterator(JigdoCache* c, const list<FilePart>::iterator& p)
      : cache(c), part(p) { }
    JigdoCache* cache;
    list<FilePart>::iterator part;
  };
  friend class JigdoCache::iterator;
  /// First element of the JigdoCache
  inline iterator begin();
  /** NB the list auto-extends, so the value of end() may change while
      you iterate over a JigdoCache. */
  iterator end() { return iterator(this, files.end()); }
  //____________________

private:
  // Read one filename from recurseDir and (if success) add entry to "files"
  void addFile(const string& name);
  /// Default reporter: Only prints error messages to stderr
  static ProgressReporter noReport;

  size_t blockLength, md5BlockLength;

  /* List of files in the cache (not vector<> because jigdo-file keeps
     ptrs, and if a vector realloc()s, all elements' addresses may
     change) */
  list<FilePart> files;
  // Equal to files.size() less any files that are deleted()
  size_t nrOfFiles;
  // Temporarily used during readFilenames()
  static struct stat fileInfo;

  // Look up LocationPath by directory name
  LocationPathSet locationPaths;

  size_t readAmount;
  vector<byte> buffer;
  ProgressReporter& reporter;

# if HAVE_LIBDB
  CacheFile* cacheFile;
  size_t cacheExpiry;
# endif
};
//______________________________________________________________________

/** Class allowing JigdoCache to convey information back to the
    creator of a JigdoCache object. */
class JigdoCache::ProgressReporter {
public:
  virtual ~ProgressReporter() { }
  /** General-purpose error reporting. NB: With JigdoCache, you can
      throw an exception inside this to abort whatever operation
      JigdoCache is doing. May be called during all checksum-returning
      methods of FilePart */
  virtual void error(const string& message);
  /** Like error(), but for purely informational messages. Default
      implementation, just like that of error(), prints message to
      cerr */
  virtual void info(const string& message);
  /** Called when the individual files are read. JigdoCache sometimes
      reads only the first md5BlockLength bytes and sometimes the
      whole file. This is *only* called when the whole file is read
      (if the file only consists of one MD5 block, it is also called).
      @param file File being scanned, or null if not applicable
      (Contains filename info). Default version does nothing at all -
      you need to supply an object of a derived class to JigdoCache()
      to act on this.
      @param offInFile Offset in this file */
  virtual void scanningFile(const FilePart* file, uint64 offInFile);
};
//______________________________________________________________________

FilePart::FilePart(LocationPathSet::iterator p, string rest, uint64 fSize,
                   time_t fMtime)
  : path(p), pathRest(rest), fileSize(fSize), fileMtime(fMtime),
    rsyncSum(), sums(), md5Sum(), flags(EMPTY) {
  //pathRest.reserve(0);
}

const string& FilePart::getPath() const {
  Paranoid(!deleted());
  return path->getPath();
}

const string& FilePart::leafName() const {
  Paranoid(!deleted());
  return pathRest;
}

uint64 FilePart::size() const {
  Paranoid(!deleted());
  return fileSize;
}

time_t FilePart::mtime() const {
  Paranoid(!deleted());
  return fileMtime;
}

const MD5* FilePart::getSums(JigdoCache* c, size_t blockNr) {
  Paranoid(!deleted());
  if (mdValid() || (blockNr == 0 && !sums.empty())) return &sums[blockNr];
  else return getSumsRead(c, blockNr);
}

const MD5Sum* FilePart::getMD5Sum(JigdoCache* c) {
  Paranoid(!deleted());
  if (mdValid()) return &md5Sum;
  else return getMD5SumRead(c);
}

const RsyncSum64* FilePart::getRsyncSum(JigdoCache* c) {
  Paranoid(!deleted());
  if (!rsyncValid()) {
    if (getSumsRead(c, 0) == 0) return 0;
  }
  Paranoid(rsyncValid());
  return &rsyncSum;
}

void FilePart::markAsDeleted(JigdoCache* c) {
  fileSize = 0; sums.resize(0); --(c->nrOfFiles);
}

void FilePart::setFlag(Flags f) {
  flags = static_cast<Flags>(static_cast<unsigned>(flags)
                             | static_cast<unsigned>(f));
}

void FilePart::clearFlag(Flags f) {
  flags = static_cast<Flags>(static_cast<unsigned>(flags)
                             & ~static_cast<unsigned>(f));
}
//________________________________________

void JigdoCache::setReadAmount(size_t bytes) {
  if (bytes < 64*1024) bytes = 64*1024;
  readAmount = bytes;
}

template <class RecurseDir>
void JigdoCache::readFilenames(RecurseDir& rd) {
  string name;
  while (true) {
    bool status = rd.getName(name, &fileInfo); // Might throw error
    if (status == FAILURE) return; // No more names
    if (fileInfo.st_size == 0) continue; // Skip zero-length files
    addFile(name);
  }
}

JigdoCache::iterator JigdoCache::begin() {
  list<FilePart>::iterator i = files.begin();
  while (i != files.end() && i->deleted()) ++i;
  return iterator(this, i);
}

JigdoCache::iterator& JigdoCache::iterator::operator++() {
  list<FilePart>::iterator end = cache->files.end();
  do ++part; while (part != end && part->deleted());
  return *this;
}
//______________________________________________________________________

template <class StringP, class StringL, class StringU>
LocationPath::LocationPath(const StringP& p, const StringL& l,
                           const StringU& u)
  : path(p), label(l), uri(u) {
  //path.reserve(0); label.reserve(0); uri.reserve(0);
}
//________________________________________

/* NB: If an entry with the given path already exists, we must not
   delete it and create another, because it is referenced from at
   least one FilePart. Because of our version of
   LocationPath::operator<, only the path matters during the find(). */
template <class StringP, class StringL, class StringU>
const LocationPathSet::iterator
    JigdoCache::addLabel(const StringP& path, const StringL& label,
                         const StringU& uri) {
  LocationPath tmp(path, label, uri);
  if (!tmp.path.empty() && tmp.path[tmp.path.length() - 1] != DIRSEP)
    tmp.path += DIRSEP;
  LocationPathSet::iterator i = locationPaths.find(tmp);
  if (i == locationPaths.end()) {
    return locationPaths.insert(tmp).first; // Create new entry
  } else {
    implicit_cast<string>(i->label) = tmp.label; // Overwrite old entry
    implicit_cast<string>(i->uri) = tmp.uri;
    return i;
  }
}

#endif

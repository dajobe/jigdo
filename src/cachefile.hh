/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

/** @file

  Cache with MD5 sums of file contents - used by JigdoCache in scan.hh

  The created libdb3 database contains one table with a mapping from
  filenames (without trailing zero byte) to a binary structure. The
  filename key is the second part of the complete filename, i.e. the
  part after any "//", as returned by FilePart::leafName(). The binary
  structure has the following format:

  This is accessed and interpreted by CacheFile:<pre>
  Size Meaning
   4   lastAccess - timestamp of last read or write access to this data
   4   fileMtime - timestamp to detect modifications, and for entry expiry
   6   fileSize - for calculation of nr of blocks, and for entry expiry</pre>

  This is not handled by CacheFile; it is passed as an opaque string of
  bytes to scan.hh classes:<pre>
   4   blockLength (of rsync sum)
   4   md5BlockLength
   4   blocks (number of valid md5 blocks in this entry)
   8   rsyncSum of file start (only valid if blocks > 0)
  16   fileMD5Sum (only valid if
                   blocks == (fileSize+md5BlockLength-1)/md5BlockLength )
  followed by n entries:
  16   md5sum of block of size md5BlockLength</pre>

  Why is mtime and size not part of the key? Because we only want to
  store one entry per file, not an additional entry whenever the file
  is changed.

*/

#ifndef CACHEFILE_HH
#define CACHEFILE_HH

#include <config.h>

#include <string>
#include <time.h> /* for time_t */

#if HAVE_LIBDB
#include <db.h>
#include <stdlib.h> /* free() */
#include <string.h> /* memcpy(), memset() */

#include <debug.hh>
#include <status.hh>
//______________________________________________________________________

/** libdb errors */
struct DbError : public Error {
  explicit DbError(int c) : Error(db_strerror(c)), code(c) { }
  DbError(int c, const string& m) : Error(m), code(c) { }
  DbError(int c, const char* m) : Error(m), code(c) { }
  int code;
};
//______________________________________________________________________

/** Cache with MD5 sums of file contents */
class CacheFile {
public:
  /** Create new database or open existing database */
  CacheFile(const char* dbName);
  inline ~CacheFile();

  /** Look for an entry in the database which matches the specified filename
      (which must be absolute), file modification time and file size. If no
      entry is found, return FAILED. Otherwise, return OK and overwrite
      resultData/resultSize with ptr/len of the binary string associated with
      this file. The first byte of resultData is the first byte of the
      "blockLength" entry (see start of this file). The result pointer is
      only valid until the next database operation. */
  Status find(const byte*& resultData, size_t& resultSize,
              const string& fileName, uint64 fileSize, time_t mtime);

  /** Look for an entry in the database which matches the specified filename
      (which must be absolute).
      If no entry is found, return FAILED. Otherwise, return OK and overwrite
      resultData/resultSize with ptr/len of the binary string associated with
      this file. The first byte of resultData is the first byte of the
      "blockLength" entry (see start of this file). The result pointer is
      only valid until the next database operation. */
  Status findName(const byte*& resultData, size_t& resultSize,
                  const string& fileName,
                  long long int& resultFileSize, time_t& resultMtime);

  /** Insert/overwrite entry for the given file (name must be
      absolute, file must have the supplied mtime and size). The data
      for the entry is supplied in inData. */
  inline void insert(const byte* inData, size_t inSize,
                     const string& fileName, time_t mtime, uint64 fileSize);
  /** As above, but data is created by the supplied functor object,
      which must have the method 'void operator()(byte* x)' defined,
      which when called must write inSize bytes to the memory at x. */
  template<class Functor>
  inline void insert(Functor f, size_t inSize, const string& fileName,
                     time_t mtime, uint64 fileSize);

  /** Remove all entries from the database that have a "last access"
      time that is older than the given time. */
  void expire(time_t t);

private:
  // Don't copy
  explicit inline CacheFile(const CacheFile&);
  inline CacheFile& operator=(const CacheFile&);
  byte* insert_prepare(size_t inSize);
  void insert_perform(const string& fileName, time_t mtime, uint64 fileSize);

  /* Byte offsets of first members of a cache entry (see start of this
     file). Defining a struct with byte members would be more
     convenient, but would also be a recipe for disaster because of
     alignment problems. (E.g., a RISC machine might pad to the next
     multiple of 4 bytes after every byte member.) */
  enum { ACCESS = 0, MTIME = 4, SIZE = 8, USER_DATA = 14 };

  DB* db; // Database object
  DBT data; // Object for result data
};
//______________________________________________________________________

CacheFile::~CacheFile() {
  free(data.data);
  Paranoid(db != 0);
  db->close(db, 0); // no flags, ignore any errors
}
//______________________________________________________________________

void CacheFile::insert(const byte* inData, size_t inSize,
    const string& fileName, time_t mtime, uint64 fileSize) {
  memcpy(insert_prepare(inSize), inData, inSize);
  insert_perform(fileName, mtime, fileSize);
}

template<class Functor>
void CacheFile::insert(Functor f, size_t inSize, const string& fileName,
                       time_t mtime, uint64 fileSize) {
  f(insert_prepare(inSize));
  insert_perform(fileName, mtime, fileSize);
}

//======================================================================

#else

// !HAVE_LIBDB - provide a dummy implementation which does nothing

class CacheFile {
public:
  CacheFile(const char*) { }
  ~CacheFile() { }
  bool find(const byte*&, size_t&, const string&, time_t, uint64) {
    return false;
  }
  bool find_name(const byte*&, size_t&, const string&,
                 long long int&, time_t&) {
    return false;
  }
  template<class Functor>
  void insert(Functor, size_t, const string&, time_t, uint64) { }
  void insert(const byte*, size_t, const string&, time_t, uint64) { }
  void expire(time_t) { }
};

#endif
#endif

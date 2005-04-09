/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Cache with MD5 sums of file contents - used by JigdoCache in scan.hh

*/

#include <config.h>

#include <cachefile.hh>
#include <compat.hh>
#if HAVE_LIBDB

#if DEBUG
#  include <iostream>
#endif
#include <new>
#include <time.h> /* time() */

#include <debug.hh>
#include <log.hh>
#include <serialize.hh>
//______________________________________________________________________

DEBUG_UNIT("cachefile")

CacheFile::CacheFile(const char* dbName) {
  memset(&data, 0, sizeof(DBT));

  int e = db_create(&db, 0, 0); // No env/flags
  if (e != 0) throw DbError(e);

  // Cache of 0GB+4MB, one contiguous chunk
  db->set_cachesize(db, 0, 4*1024*1024, 1);

  // Use a btree, create database file if not yet present
  e = compat_dbOpen(db, dbName, "jigdo filecache v0", DB_BTREE, DB_CREATE,
                    0666);
  if (e != 0) {
    // Re-close, in case it is necessary
    db->close(db, 0);
    if (e != DB_OLD_VERSION && e != DB_RUNRECOVERY)
      throw DbError(e);
    /* If the DB file is old or corrupted, just regenerate it from
       scratch, otherwise throw error. */
    debug("Cache file corrupt, recreating it");
    if (compat_dbOpen(db, dbName, "jigdo filecache v0", DB_BTREE,
                      DB_CREATE | DB_TRUNCATE, 0666) != 0)
      throw DbError(e);
  }

  data.flags |= DB_DBT_REALLOC;
}
//______________________________________________________________________

namespace {

  /** Local struct: Wrapper which calls close() for any DBC cursor at end of
      scope */
  struct AutoCursor {
    AutoCursor() : c(0) { }
    ~AutoCursor() { close(); }
    int close() {
      if (c == 0) return 0;
      int r = c->c_close(c);
      c = 0;
      return r;
    }
    int get(DBT *key, DBT *data, u_int32_t flags) {
      return c->c_get(c, key, data, flags);
    }
    int put(DBT *key, DBT *data, u_int32_t flags) {
      return c->c_put(c, key, data, flags);
    }
    int del(u_int32_t flags) {
      return c->c_del(c, flags);
    }
    DBC* c;
  };

}
//________________________________________

bool CacheFile::find(const byte*& resultData, size_t& resultSize,
                     const string& fileName, uint64 fileSize, time_t mtime) {
  DBT key; memset(&key, 0, sizeof(DBT));
  key.data = const_cast<char*>(fileName.c_str());
  key.size = fileName.size();

  AutoCursor cursor;
  // Cursor with no transaction id, no flags
  if (db->cursor(db, 0, &cursor.c, 0) != 0) return false;

  if (cursor.get(&key, &data, DB_SET) == DB_NOTFOUND
      || data.data == 0) return false;

  // Check whether mtime and size matches
  Paranoid(data.size >= USER_DATA);
  byte* d = static_cast<byte*>(data.data);
  Paranoid(d != 0);
  time_t cacheMtime;
  unserialize4(cacheMtime, d + MTIME);
  if (cacheMtime != mtime) return false;
  uint64 cacheFileSize;
  unserialize6(cacheFileSize, d + SIZE);
  if (cacheFileSize != fileSize) return false;

  // Match - update access time
  time_t now = time(0);
  Paranoid(now != static_cast<time_t>(-1));
  serialize4(now, d + ACCESS);
  DBT partial; memset(&partial, 0, sizeof(DBT));
  partial.data = d + ACCESS;
  partial.size = 4;
  partial.flags |= DB_DBT_PARTIAL;
  partial.doff = ACCESS;
  partial.dlen = 4;
  //cerr << "CacheFile lookup successfull for "<<fileName<<endl;
  cursor.put(&key, &partial, DB_CURRENT);

  resultData = d + USER_DATA;
  resultSize = data.size - USER_DATA;
  return true;
}
//______________________________________________________________________

void CacheFile::expire(time_t t) {
  DBT key; memset(&key, 0, sizeof(DBT));
  DBT data; memset(&data, 0, sizeof(DBT));
  AutoCursor cursor;
  // Cursor with no transaction id, no flags
  if (db->cursor(db, 0, &cursor.c, 0) != 0) return;

  int status;
  while ((status = cursor.get(&key, &data, DB_NEXT)) == 0) {
    time_t lastAccess = 0;
    // If data.data == 0, expire entry by leaving lastAccess at 0
    if (data.data != 0)
      unserialize4(lastAccess, static_cast<byte*>(data.data) + ACCESS);
    // Same as 'if (lastAccess<t)', but deals with wraparound:
    if (static_cast<signed>(t - lastAccess) > 0) {
      debug("Cache: expiring %1",
            string(static_cast<char*>(key.data), key.size));
      cursor.del(0);
    }
  }
  if (status != DB_NOTFOUND)
    throw DbError(status);
}
//______________________________________________________________________

/* Prepare for an insertion of data, by allocating a sufficient amount
   of memory and returning a pointer to it. */
byte* CacheFile::insert_prepare(size_t inSize) {
  // Allocate enough memory for the new entry
  void* tmp = realloc(data.data, USER_DATA + inSize);
  if (tmp == 0) throw bad_alloc();
  data.data = tmp;
  data.size = USER_DATA + inSize;
  return static_cast<byte*>(tmp) + USER_DATA;
}

/* ASSUMES THAT insert_prepare() HAS JUST BEEN CALLED and that the
   data had been copied to the memory region it returned. This
   function commits the data to the db. */
void CacheFile::insert_perform(const string& fileName, time_t mtime,
                               uint64 fileSize) {
  byte* buf = static_cast<byte*>(data.data);

  // Write our data members
  time_t now = time(0);
  serialize4(now, buf + ACCESS);
  serialize4(mtime, buf + MTIME);
  serialize6(fileSize, buf + SIZE);

  // Insert in database
  DBT key; memset(&key, 0, sizeof(DBT));
  key.data = const_cast<char*>(fileName.c_str());
  key.size = fileName.size();
  db->put(db, 0, &key, &data, 0); // No transaction, overwrite

//   cerr << "CacheFile write `"<<fileName<<'\'';
//   for (size_t i = USER_DATA; i < data.get_size(); ++i)
//     cerr << ' '<< hex << (int)(buf[i]);
//   cerr << endl;
}
//______________________________________________________________________

#endif /* HAVE_LIBDB */

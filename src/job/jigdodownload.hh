/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download .jigdo data, take care of handling [Include] directives.

*/

#ifndef JIGDODOWNLOAD_HH
#define JIGDODOWNLOAD_HH

#include <config.h>

#include <makeimagedl.hh>
//______________________________________________________________________

// PRIVATE stuff, internal to MakeImageDl

/** Private class of MakeImageDl: Object extending a SingleUrl and used to
    retrieve the data of the .jigdo file.

    NB: The underlying Download's io pointer is set up to point to this
    object. Once this receives the calls, this object passes them on to its
    own io pointer, which will point to the corresponding front-end object,
    e.g. a GtkSingleUrl. This sandwiching is in contrast to "normal"
    single-file downloads, where the Download directly calls the
    GtkSingleUrl. */
class Job::MakeImageDl::JigdoDownload
    : SingleUrl, // This object is a special kind of SingleUrl
      SingleUrl::IO,
      Gunzip::IO {
public:
  /** @param m Master which owns us
      @param p Parent JigdoDownload which [Include]d us, or null
      @param jigdoUrl Where to download .jigdo data
      @param destPos Where in config file to insert downloaded data */
  JigdoDownload(MakeImageDl* m, JigdoDownload* p, const string& jigdoUrl,
                ConfigFile::iterator destPos);
  virtual ~JigdoDownload();

  /** Access the correct io member, i.e. for the derived class. */
  virtual IOPtr<SingleUrl::IO>& io();
  virtual const IOPtr<SingleUrl::IO>& io() const;

private:
  /** Methods from SingleUrl::IO */
  virtual void job_deleted();
  virtual void job_succeeded();
  virtual void job_failed(string* message);
  virtual void job_message(string* message);
  virtual void singleUrl_dataSize(uint64 n);
  virtual void singleUrl_data(const byte* data, size_t size,
                              uint64 currentSize);

  // Virtual methods from Gunzip::IO
  virtual void gunzip_deleted();
  virtual void gunzip_data(Gunzip*, byte* decompressed, unsigned size);
  virtual void gunzip_needOut(Gunzip*);
  virtual void gunzip_failed(string* message);

  // Convenience helper function
  inline ConfigFile& configFile() const;

  MakeImageDl* master; // Ptr to the MakeImageDl which owns us
  JigdoDownload* parent; // Ptr to the download which [Include]d us, or null
  // IO for this SingleUrl, given by master. Points to e.g. a GtkSingleUrl
  IOPtr<Job::SingleUrl::IO> ioVal;

  /* Transparent gunzipping of .jigdo file. GUNZIP_BUF_SIZE is also the max
     size a single line in the .jigdo is allowed to have */
  static const unsigned GUNZIP_BUF_SIZE = 16384;
  byte gunzipBuf[GUNZIP_BUF_SIZE];
  Gunzip gunzip;

  // Where to put .jigdo data. Points somewhere inside configFile()
  ConfigFile::iterator insertPos;
};

ConfigFile& Job::MakeImageDl::JigdoDownload::configFile() const {
  return master->mi.configFile();
}

#endif

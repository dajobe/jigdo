/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  IO object for .jigdo downloads; download, gunzip, interpret

  Data (=downloaded bytes, status info) flows as follows:
  Download -> SingleUrl -> JigdoIO -> GtkSingleUrl

  The JigdoIO owns the SingleUrl (and the Download *object* inside it), but
  it doesn't own the GtkSingleUrl.

*/

#ifndef JIGDO_IO_HH
#define JIGDO_IO_HH

#include <datasource.hh>
#include <makeimagedl.fh>
#include <nocopy.hh>
//______________________________________________________________________

namespace Job {
  class JigdoIO;
}

class Job::JigdoIO : NoCopy, public Job::DataSource::IO {
public:
  /** Create a new JigdoIO which is owned by m, gets data from download (will
      register itself with download's IO object) and passes it on to
      childIo. */
  JigdoIO(MakeImageDl* m, DataSource* download,
          DataSource::IO* childIo);
  ~JigdoIO();
  virtual Job::IO* job_removeIo(Job::IO* rmIo);
  virtual void job_deleted();
  virtual void job_succeeded();
  virtual void job_failed(string* message);
  virtual void job_message(string* message);
  virtual void dataSource_dataSize(uint64 n);
  virtual void dataSource_data(const byte* data, size_t size,
                               uint64 currentSize);
private:
  MakeImageDl* master;
  DataSource* source;
  DataSource::IO* child;
};
//______________________________________________________________________

#endif

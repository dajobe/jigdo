/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Interface for objects returning data from the network or from disk

*/

#ifndef DATASOURCE_HH
#define DATASOURCE_HH

#include <job.hh>
#include <nocopy.hh>
//______________________________________________________________________

namespace Job {
  class DataSource;
}

/** Interface implemented by SingeUrl and CachedUrl. */
class Job::DataSource : NoCopy {
public:
  class IO;
};
//______________________________________________________________________

/** User interaction for DataSource. */
class Job::DataSource::IO : public Job::IO {
public:

  /** Called by the job when it is deleted or when a different IO object is
      registered with it. If the IO object considers itself owned by its job,
      it can delete itself. */
  virtual void job_deleted() = 0;

  /** Called when the job has successfully completed its task. */
  virtual void job_succeeded() = 0;

  /** Called when the job fails. The only remaining action after getting this
      is to delete the job object. */
  virtual void job_failed(string* message) = 0;

  /** Informational message. */
  virtual void job_message(string* message) = 0;

  /** Called as soon as the size of the downloaded data is known. May not be
      called at all if the size is unknown.
      Problem with libwww: Returns size as long int - 2 GB size limit! */
  virtual void dataSource_dataSize(uint64 n) = 0;

  /** Called during download whenever data arrives, with the data that just
      arrived. You can write the data to a file, copy it away etc.
      currentSize is the offset into the downloaded data (including the
      "size" new bytes) - useful for "x% done" messages. */
  virtual void dataSource_data(const byte* data, size_t size,
                              uint64 currentSize) = 0;
};

#endif

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  IO object for .jigdo downloads; download, gunzip, interpret

*/

#include <config.h>
#include <jigdo-io.hh>
#include <makeimagedl.hh>
//______________________________________________________________________

using namespace Job;

JigdoIO::JigdoIO(MakeImageDl* m, DataSource* download,
                 DataSource::IO* childIo)
    : master(m), source(download), child(childIo) {
  download->io.set(this);
}
//______________________________________________________________________

JigdoIO::~JigdoIO() {
  delete source;
}
//______________________________________________________________________

Job::IO* JigdoIO::job_removeIo(Job::IO* rmIo) {
  if (rmIo == this) {
    if (child != 0) master->io->makeImageDl_finished(source, child);
    IO* c = child;
    // Do not "delete this" - we own the SingleUrl, not the other way round
    return c;
  } else if (child != 0) {
    Job::IO* c = child->job_removeIo(rmIo);
    Paranoid(c == 0 || dynamic_cast<IO*>(c) != 0);
    child = static_cast<IO*>(c);
  }
  return this;
}

void JigdoIO::job_deleted() {
  if (child != 0) child->job_deleted();
  // Do not "delete this" - we own the SingleUrl, not the other way round
}

void JigdoIO::job_succeeded() {
  if (child != 0) {
    child->job_succeeded();
    master->io->makeImageDl_finished(source, child);
  }
}

void JigdoIO::job_failed(string* message) {
  if (child != 0) {
    child->job_failed(message);
    master->io->makeImageDl_finished(source, child);
  }
}

void JigdoIO::job_message(string* message) {
  if (child != 0) child->job_message(message);
}

void JigdoIO::dataSource_dataSize(uint64 n) {
  if (child != 0) child->dataSource_dataSize(n);
}

void JigdoIO::dataSource_data(const byte* data, size_t size,
                              uint64 currentSize) {
  if (child != 0) child->dataSource_data(data, size, currentSize);
}

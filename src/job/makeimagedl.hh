/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download .jigdo/.template and file URLs. MakeImageDl takes care of
  downloading the data from the appropriate places and then passes it to its
  private MakeImage member. See also makeimage.hh.

*/

#ifndef MAKEIMAGEDL_HH
#define MAKEIMAGEDL_HH

#include <config.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include <datasource.hh>
#include <gunzip.hh>
#include <job.hh>
#include <makeimage.hh>
#include <makeimagedl.fh>
#include <md5sum.hh>
#include <nocopy.hh>
#include <single-url.hh>
//______________________________________________________________________

/**
    MakeImageDl: Everything related to downloads
    <ul>

      <li>Pushes .jigdo file contents from a SingleURL to the MakeImage

      <li>Downloads .template via SingleURL, notifies MakeImage once done

      <li>"Owner" of the layout of the temporary directory, only component
      that directly makes modifications to this dir. (MakeImage only writes
      to the image file.)

      <li>Does simple cache management; if requested file already downloaded,
      immediately returns its data, or does an If-Modified-Since request; if
      partially downloaded, resumes.

      <li>Starts further SingleURLs for download of individual parts.

      <li>Automatic server selection: For servers which were rated equally
      acceptable by the user, measures their speed, then prefers the faster
      ones (but does not completely stop using the slower ones).

    </ul>

    See comments in makeimage.hh for the big picture. */
class Job::MakeImageDl : NoCopy, public JigdoConfig::ProgressReporter {
public:
  class IO;
  IOPtr<IO> io; // Points to e.g. a GtkMakeImage

  /** Maximum allowed [Include] directives in a .jigdo file and the files it
      includes. Once exceeded, io->job_failed() is called. */
  static const int MAX_INCLUDES = 100;

  /** Maximum depth of [Include] directives, mainly to detect include loops.
      Once exceeded, io->job_failed() is called. */
  static const int MAX_INCLUDE_DEPTH = 10;

  enum State {
    DOWNLOADING_JIGDO,
    DOWNLOADING_TEMPLATE,
    FINAL_STATE, // Value isn't actually used; all below are final states:
    ERROR
  };

  /** Prepare object. No download is started yet.
      @param destination Name of directory to create tmp directory in, for
      storing data during the download. */
  MakeImageDl(IO* ioPtr, const string& jigdoUri, const string& destination);

  /** Destroy this MakeImageDl and all its children. */
  ~MakeImageDl();

  /** Start downloading. First creates a new download for the .jigdo data,
      then the .template data, etc. */
  void run();

  inline const string& jigdoUri() const;

  /** Return current state of object. */
  inline State state() const;

  /** Return name of temporary directory. This is a subdir of "destination"
      (ctor arg), contains a hash of the jigdoUri. Never ends in '/'. */
  inline const string& tmpDir() const;

private:
  /** Methods from JigdoConfig::ProgressReporter */
  virtual void error(const string& message);
  virtual void info(const string& message);

  // Write a ReadMe.txt to the download dir; fails silently
  void writeReadMe();

  /** Return an object which returns the data of the requested URL. That
      returned object is usually a newly started download, except if the file
      (or its beginning) was already downloaded. The filename is based on
      either the base64ified md checksum, or (if that is 0), the b64ied md5
      checksum of the url.
      @return New object, or null if error (and io->job_failed was called) */
  DataSource* dataSourceFor(const string& url, const MD5* md = 0);
  // Helper methods for above
  DataSource* dataSourceForCompleted(const struct stat& fileInfo,
                                     const string& filename, const MD5* md);
  DataSource* dataSourceForSemiCompleted(const struct stat& fileInfo,
                                         const string& filename);

  // Wraps around a SingleUrl, for downloading the .jigdo file
  class JigdoDownload;
  friend class JigdoDownload;

  // Set state to ERROR and call io->job_failed
  inline void generateError(string* message, State newState = ERROR);
  // Return true if current state is final
  inline bool finalState() const;

  State stateVal; // State, e.g. "downloading jigdo file", "error"

  string jigdoUrl; // URL of .jigdo file
  Job::JigdoIO* jigdo; // .jigdo file; may contain tree of [Include]d files

  string dest; // Destination dir. No trailing '/', empty string for root dir
  string tmpDirVal; // Temporary dir, a subdir of dest

  // Workhorse which actually generates the image from the data we feed it
  MakeImage mi;
};
//______________________________________________________________________

/** User interaction for MakeImageDl. */
class Job::MakeImageDl::IO : public Job::IO {
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

  /** Called by MakeImageDl after it has started a new download; either
      the download of the .jigdo file or the .template file, or the download
      of a part. The GTK+ GUI uses this to display the new SingleUrl as a
      "child" of the MakeImageDl.
      @param childDownload For example a SingleUrl, but could also be an
      object which just outputs existing cache contents.
      @param destDesc A descriptive string like "/foo/bar/image, offset
      3453", NOT a filename! Supplied for information only, to be displayed
      to the user.
      @return IO object to associate with this child download. Anything
      happening to the SingleUrl child will be passed on to the object you
      return here. Can return null if nothing should be called, but this
      won't prevent the child download from being created. */
  virtual Job::DataSource::IO* makeImageDl_new(
      Job::DataSource* childDownload, const string& destDesc) = 0;

  /** Usually called when the child has (successfully or not) finished, i.e.
      just after yourIo->job_succeeded/failed() was called. childDownload
      will be deleted after calling this.

      Also called if you have a JigdoIO chained in front of a DataSource::IO
      and then IOPtr::remove() the JigdoIO - however, this second case should
      not happen with the current code. :)

      @param yourIo The value you returned from makeImageDl_new() */
  virtual void makeImageDl_finished(Job::DataSource* childDownload,
                                    Job::DataSource::IO* yourIo) = 0;
};
//______________________________________________________________________

const string& Job::MakeImageDl::jigdoUri() const { return jigdoUrl; }

Job::MakeImageDl::State Job::MakeImageDl::state() const { return stateVal; }

void Job::MakeImageDl::generateError(string* message, State newState) {
  stateVal = newState;
  if (io) io->job_failed(message);
}

bool Job::MakeImageDl::finalState() const { return state() > FINAL_STATE; }

const string& Job::MakeImageDl::tmpDir() const { return tmpDirVal; }

#endif

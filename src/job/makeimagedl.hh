/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

  Download .jigdo/.template and file URLs. MakeImageDl takes care of
  downloading the data from the appropriate places and then passes it to its
  private MakeImage member. See also makeimage.hh.

*/

#ifndef MAKEIMAGEDL_HH
#define MAKEIMAGEDL_HH

#include <config.h>

#include <glib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include <datasource.hh>
#include <ilist.hh>
#include <job.hh>
#include <makeimage.hh>
#include <makeimagedl.fh>
#include <md5sum.hh>
#include <nocopy.hh>
#include <single-url.hh>
#include <status.hh>
#include <url-mapping.fh>
//______________________________________________________________________

/**
    MakeImageDl: Everything related to downloads
    <ul>

      <li>Downloads and interprets .jigdo file contents

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
class Job::MakeImageDl : NoCopy/*, public JigdoConfig::ProgressReporter*/ {
public:
  class Child;
  class IO;
  IOPtr<IO> io; // Points to e.g. a GtkMakeImage
  //____________________

  /** Maximum allowed [Include] directives in a .jigdo file and the files it
      includes. Once exceeded, io->job_failed() is called. */
  static const int MAX_INCLUDES = 100;

  enum State {
    DOWNLOADING_JIGDO,
    DOWNLOADING_JIGDO_TEMPLATE,
    DOWNLOADING_TEMPLATE,
    FINAL_STATE, // Value isn't actually used; all below are final states:
    ERROR
  };

  /** Return the string "Cache entry %1 -- %2" or an equivalent localized
      string. This is used for the destination description of children. */
  inline static const char* destDescTemplate();

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

  /** Set state to ERROR and call io->job_failed */
  inline void generateError(string* message, State newState = ERROR);
  /** Return true if current state is final */
  inline bool finalState() const;

  /** To be called by implementers of DataSource::IO only: Notify this object
      that the download is complete and that all bytes have been received.
      Will also call io->makeImageDl_finished(). Either this or childFailed()
      below MUST eventually be called by the IO object.
      @param child The SingleUrl or similar whose IO pointer points to
      childIo
      @param childIo The "this" pointer of the DataSource::IO implementer.
      @param frontendIo The IO object to which childIo forwards calls; e.g.
      GtkSingleUrl. */
  void childSucceeded(Child* childDl, DataSource::IO* childIo,
                      DataSource::IO* frontend);
  /** As above, but notify this object that the download has failed, not all
      bytes have been received. */
  void childFailed(Child* childDl, DataSource::IO* childIo,
                   DataSource::IO* frontend);

  /** Is called by a child JigdoIO once the [Image] section has been seen.
      The arguments are modified! */
  void setImageSection(string* imageName, string* imageInfo,
                       string* imageShortInfo, string* templateUrl,
                       MD5** templateMd5);
  /** Has setImageSection() already been called? (=>is imageName
      non-empty?) */
  inline bool haveImageSection() const;

  /** Add info about a mapping line inside one of the [Parts] sections in the
      .jigdo sections. The first entry of "value" is the URL (absolute,
      relative to baseUrl or in "Label:some/path form). The remaining "value"
      entries are assumed to be options, and ignored ATM. */
  void addPart(const string& baseUrl, const MD5& md, vector<string>& value);

  /** Add info about a [Servers] line, cf addPart(). For a line
      "Foobar=Label:some/path" in the [Servers] section:
      @param label == "Foobar"
      @param value arguments; value.front()=="Label:some/path"
      @return failed() iff the line results in a recursive server
      definition. */
  Status addServer(const string& baseUrl, const string& label,
                   vector<string>& value);

  /** Output the graph built up by addPart()/addServer() to the log. */
  void dumpJigdoInfo();

  /** Return child download object which contains a DataSource which produces
      the data of the requested URL. That returned object is usually a newly
      started download, except if the file (or its beginning) was already
      downloaded. The filename is based on either the base64ified md
      checksum, or (if that is 0), the b64ied md5 checksum of the url.
      @param leafnameOut If non-null, string is overwritten with file's
      leafname in cache on exit.
      @return New object, or null if error (and io->job_failed was called).
      If non-null, returned object will be deleted from this MakeImageDl's
      dtor (unless it is deleted earlier). */
  Child* childFor(const string& url, const MD5* md = 0,
                  string* leafnameOut = 0);

  typedef IList<Child> ChildList;
  /** Return the list of Child objects owned by this MakeImageDl. */
  inline const ChildList& children() const;

  /** Return info about the first image section, or empty strings if none
      encountered so far */
  inline const string& imageName() const { return imageNameVal; }
  inline const string& imageShortInfo() const { return imageShortInfoVal; }
  inline const string& templateUrl() const { return templateUrlVal; }
  inline const MD5* templateMd5() const { return templateMd5Val; }
  /** This one is special: The contents of ImageInfo are an XML-style string,
      with markup containing tags named: b i tt u big small br p. When
      getting the string, the frontend must specify what strings the
      respective begin and end tags should be replaced with. The default args
      turn the string into plain UTF-8 without markup. This call is fairly
      expensive, you may want to cache the returned string.

      If there in an error parsing the XML, the string from the ImageInfo
      entry is either returned unchanged (if !escapedText), or all
      "dangerous" characters are escaped (if escapedText == true);

      If there is no error, escapedText==true means that any "&lt;" in the
      normal text should be kept escaped as "&lt;". escapedText=false will
      unescape the "&lt;" to "<".

      The subst argument is of the form:
      {
        "", "", // <b>, </b>
        "", "", // <i>, </i>
        "", "", // <tt>, </tt>
        "", "", // <u>, </u>
        "", "", // <big>, </big>
        "", "", // <small>, </small>
        "", "", // <br/>
      };

      @param output String to append image info to
      @param escapedText true if the characters <>& should be escaped as
      &lt;, &gt; &amp; */
  void imageInfo(string* output, bool escapedText,
                 const char* subst[13]) const;
  /** Helper enum for the offsets above */
  enum { B, B_, I, I_, TT, TT_, U, U_, BIG, BIG_, SMALL, SMALL_, BR };

  /** Return ImageInfo as it appears in the .jigdo file. The value has *not*
      been checked for validity (correct tag nesting etc). */
  inline const string& imageInfoOrig() const { return imageInfoVal; }

  /** To be called by JigdoIO only. Called to notify the MakeImageDl that the
      last JigdoIO has successfully finished. Called just after
      childSucceeded() for that JigdoIO. */
  void jigdoFinished();

private: // Really private
  /** Methods from JigdoConfig::ProgressReporter */
//   virtual void error(const string& message);
//   virtual void info(const string& message);

  // Write a ReadMe.txt to the download dir; fails silently
  void writeReadMe();

  // Add leafname for object to arg string, e.g. "u-nGJ2hQpUNCIZ0fafwQxZmQ"
  static void appendLeafname(string* s, bool contentMd, const MD5& md);
  /* Turn the '-' in string created by above function into a '~' or v.v.
     Works even if leafname is preceded by dirname or similar in s */
  static inline void toggleLeafname(string* s);

  // Helper methods for childFor()
  Child* childForCompleted(const struct stat& fileInfo,
                           const string& filename, bool contentMdKnown,
                           const MD5& cacheMd);
  Child* childForSemiCompleted(const struct stat& fileInfo,
                               const string& filename);

  ServerUrlMapping* findOrCreateServerUrlMapping(const string& url,
                                                 unsigned colon);

  static const char* destDescTemplateVal;

  State stateVal; // State, e.g. "downloading jigdo file", "error"

  string jigdoUrl; // URL of .jigdo file
  ChildList childrenVal; // Child downloads

  string dest; // Destination dir. No trailing '/', empty string for root dir
  string tmpDirVal; // Temporary dir, a subdir of dest

  // Workhorse which actually generates the image from the data we feed it
  MakeImage mi;

  // Info about first image section of this .jigdo, if any
  string imageNameVal;
  string imageInfoVal, imageShortInfoVal;
  string templateUrlVal;
  MD5* templateMd5Val;

  /* [Parts] lines in .jigdo data; for each md5sum, there's a linked list of
     PartUrlMappings */
  typedef map<MD5, SmartPtr<PartUrlMapping> > PartMap;
  PartMap parts;
  /* [Servers] lines in .jigdo data; for each label string, there's a linked
     list of ServerUrlMappings */
  typedef map<string, SmartPtr<ServerUrlMapping> > ServerMap;
  ServerMap servers;
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
      Job::DataSource* childDownload, const string& uri,
      const string& destDesc) = 0;

  /** Usually called when the child has (successfully or not) finished, i.e.
      just after yourIo->job_succeeded/failed() was called. childDownload
      will be deleted after calling this.

      Also called if you have a JigdoIO chained in front of a DataSource::IO
      and then IOPtr::remove() the JigdoIO - however, this second case should
      not happen with the current code. :)

      @param yourIo The value you returned from makeImageDl_new() */
  virtual void makeImageDl_finished(Job::DataSource* childDownload,
                                    Job::DataSource::IO* yourIo) = 0;

  /** Called as soon as the first [Image] section in the .jigdo data has been
      parsed. Call MakeImageDl::imageName() etc to get the info from the
      image section. */
  virtual void makeImageDl_haveImageSection() = 0;
};
//______________________________________________________________________

/** Each Child object stands for one DataSource (i.e. SingleUrl/CachedUrl)
    which the MakeImageDl starts as a "child download" of itself. The Child
    maintains a private pointer to a DataSource.

    Used to store additional information which the MakeImageDl needs, e.g.
    the filename in the cache.

    The ctor and dtor automatically add/remove the Child in its master
    MakeImageDl's list of children. */
class Job::MakeImageDl::Child : NoCopy, public IListBase {
  friend class MakeImageDl;
public:
  inline ~Child();

  /** @return The MakeImageDl which owns this object. */
  MakeImageDl* master() const { return masterVal; }
  /** @return The SingleUrl/CachedUrl owned by this object. */
  inline DataSource* source() const;
  /** Delete source object; subsequently, source()==0 */
  inline void deleteSource();
  /** @return The JigdoIO or similar owned by this object. null after init */
  inline DataSource::IO* childIo() const;

  /** Only to be called by MakeImageDl and its helper classes (JigdoIO)
      @param m Master MakeImageDl
      @param listHead Pointer to master->children */
  inline Child(MakeImageDl* m, IList<Child>* listHead,
      DataSource* src, bool contentMdKnown, const MD5& mdOfContentOrUrl);
  /** Only to be called by MakeImageDl and its helper classes (JigdoIO) */
  inline void setChildIo(DataSource::IO* c);

# if DEBUG
  // childFailed() or childSucceeded() MUST always be called for a Child
  bool childSuccFail;
# endif

private: // really private
  MakeImageDl* masterVal;
  DataSource* sourceVal;
  /** Most of the time, the value of childIoVal is the same as
      sourceVal->io.get() - except when another Child has been started for
      the same URL as we and is now waiting for us to finish; to be notified
      when we finish, it'll insert another IO in sourceVal->io. */
  DataSource::IO* childIoVal;
  bool contentMd; // True <=> md is checksum of file contents (else of URL)
  MD5 md;
};
//______________________________________________________________________

const char* Job::MakeImageDl::destDescTemplate() {
  return destDescTemplateVal;
}

const string& Job::MakeImageDl::jigdoUri() const { return jigdoUrl; }

Job::MakeImageDl::State Job::MakeImageDl::state() const { return stateVal; }

void Job::MakeImageDl::generateError(string* message, State newState) {
  if (finalState()) return;
  stateVal = newState;
  if (io) io->job_failed(message);
}

bool Job::MakeImageDl::finalState() const { return state() > FINAL_STATE; }

const string& Job::MakeImageDl::tmpDir() const { return tmpDirVal; }

void Job::MakeImageDl::toggleLeafname(string* s) {
  int off = s->length() - 23;
  Paranoid((*s)[off] == '-' || (*s)[off] == '~');
  (*s)[off] ^= ('-' ^ '~');
}

bool Job::MakeImageDl::haveImageSection() const {
  return !imageNameVal.empty();
}

const Job::MakeImageDl::ChildList& Job::MakeImageDl::children() const {
  return childrenVal;
}
//____________________

Job::MakeImageDl::Child::Child(MakeImageDl* m, IList<Child>* list,
    DataSource* src, bool contentMdKnown, const MD5& mdOfContentOrUrl)
  : masterVal(m), sourceVal(src), childIoVal(0),
    contentMd(contentMdKnown), md(mdOfContentOrUrl) {
  Paranoid(list != 0);
  list->push_back(*this);
# if DEBUG
  //msg("Child %1", this);
  childSuccFail = false;
# endif
}

Job::MakeImageDl::Child::~Child() {
# if DEBUG
  //msg("~Child %1", this);
  Paranoid(childSuccFail);
# endif
  deleteSource();
  delete childIoVal;
}

Job::DataSource* Job::MakeImageDl::Child::source() const {
  return sourceVal;
}

Job::DataSource::IO* Job::MakeImageDl::Child::childIo() const {
  return childIoVal;
}

void Job::MakeImageDl::Child::deleteSource() {
  delete sourceVal;
  sourceVal = 0;
}

void Job::MakeImageDl::Child::setChildIo(DataSource::IO* c) {
  Paranoid(sourceVal->io.get() == 0);
  sourceVal->io.set(c);
  Paranoid(childIoVal == 0);
  childIoVal = c;
}

#endif

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Create image from template / merge new files into image.tmp

*/

#ifndef MKIMAGE_HH
#define MKIMAGE_HH

#include <config.h>

#include <iosfwd>
#include <queue>
#include <vector>
#include <typeinfo>

#include <bstream.hh>
#include <debug.hh>
#include <md5sum.hh>
#include <scan.hh>
#include <serialize.hh>
//______________________________________________________________________

/** Errors thrown by the JigdoDesc code */
struct JigdoDescError : Error {
  explicit JigdoDescError(const string& m) : Error(m) { }
  explicit JigdoDescError(const char* m) : Error(m) { }
};

/** Entry in a DESC section of a jigdo template. This definition is
    used both for an abstract base class and as a namespace for child
    classes. */
class JigdoDesc {
public:
  /// Types of entries in a description section
  enum Type {
    IMAGE_INFO = 5, UNMATCHED_DATA = 2, MATCHED_FILE = 6, WRITTEN_FILE = 7,
    OBSOLETE_IMAGE_INFO = 1, OBSOLETE_MATCHED_FILE = 3,
    OBSOLETE_WRITTEN_FILE = 4
  };
  class ProgressReporter;
  //____________________

  virtual bool operator==(const JigdoDesc& x) const = 0;
  inline bool operator!=(const JigdoDesc& x) const { return !(*this == x); }
  virtual ~JigdoDesc() = 0;

  /// Entry type of JigdoDesc child class
  virtual Type type() const = 0;
  /// Output human-readable summary
  virtual ostream& put(ostream& s) const = 0;
  /// Size of image area or whole image
  virtual uint64 size() const = 0;

  /* There are no virtual templates, so this wouldn't work:
     template<class Iterator>
     virtual Iterator serialize(Iterator i) const; */
  virtual size_t serialSizeOf() const = 0;

  /** Check whether the file is a .template file, i.e. has the
      appropriate ASCII header */
  static bool isTemplate(bistream& file);
  /** Assuming that a DESC section is at the end of a file, set the
      file pointer to the start of the section, allowing you to call
      read() immediately afterwards. */
  static void seekFromEnd(bistream& file) throw(JigdoDescError);
  /** Create image file from template and files (via JigdoCache) */
  static int makeImage(JigdoCache* cache, const string& imageFile,
    const string& imageTmpFile, const string& templFile,
    bistream* templ, const bool optForce,
    ProgressReporter& pr = noReport, size_t readAmnt = 128U*1024,
    const bool optMkImageCheck = true);
  /** Return list of MD5sums of files that still need to be copied to
      the image to complete it. Reads info from tmp file or (if
      imageTmpFile.empty() or error opening tmp file) outputs complete
      list from template. */
  static int listMissing(set<MD5>& result, const string& imageTmpFile,
    const string& templFile, bistream* templ, ProgressReporter& reporter);

  class ImageInfo;
  class UnmatchedData;
  class MatchedFile;
  class WrittenFile;

private:
  static ProgressReporter noReport;
};

inline ostream& operator<<(ostream& s, JigdoDesc& jd) { return jd.put(s); }
//______________________________________________________________________

/// Information about the image file
class JigdoDesc::ImageInfo : public JigdoDesc {
public:
  inline ImageInfo(uint64 s, const MD5& m, size_t b);
  inline ImageInfo(uint64 s, const MD5Sum& m, size_t b);
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return IMAGE_INFO; }
  uint64 size() const { return sizeVal; }
  const MD5& md5() const { return md5Val; }
  size_t blockLength() const { return blockLengthVal; }
  // Default dtor, operator==
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;

private:
  uint64 sizeVal;
  MD5 md5Val;
  size_t blockLengthVal;
};
//________________________________________

/** Info about data that was not matched by any input file, i.e.
    that is included in the template data verbatim */
class JigdoDesc::UnmatchedData : public JigdoDesc {
public:
  UnmatchedData(uint64 o, uint64 s) : offsetVal(o), sizeVal(s) { }
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return UNMATCHED_DATA; }
  uint64 offset() const { return offsetVal; }
  uint64 size() const { return sizeVal; }
  void resize(uint64 s) { sizeVal = s; }
  // Default dtor, operator==
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;

private:
  uint64 offsetVal; // Offset in image
  uint64 sizeVal;
};
//________________________________________

/// Info about data that *was* matched by an input file
class JigdoDesc::MatchedFile : public JigdoDesc {
public:
  inline MatchedFile(uint64 o, uint64 s, const RsyncSum64& r, const MD5& m);
  inline MatchedFile(uint64 o, uint64 s, const RsyncSum64& r,
                     const MD5Sum& m);
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return MATCHED_FILE; }
  uint64 offset() const { return offsetVal; }
  uint64 size() const { return sizeVal; }
  const MD5& md5() const { return md5Val; }
  const RsyncSum64& rsync() const { return rsyncVal; }
  // Default dtor, operator==
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;

private:
  uint64 offsetVal; // Offset in image
  uint64 sizeVal;
  RsyncSum64 rsyncVal;
  MD5 md5Val;
};
//________________________________________

/** Like MatchedFile - used only in .tmp files to express that the
    file data was successfully written to the image. NB: Because this
    derives from MatchedFile and because of the implementation of
    JigdoDesc::operator==, MatchedFile's and WrittenFile's will
    compare equal if their data fields are identical. */
class JigdoDesc::WrittenFile : public MatchedFile {
public:
  WrittenFile(uint64 o, uint64 s, const RsyncSum64& r, const MD5& m)
    : MatchedFile(o, s, r, m) { }
  // Implicit cast to allow MatchedFile and WrittenFile to compare equal
  inline bool operator==(const JigdoDesc& x) const;
  Type type() const { return WRITTEN_FILE; }
  virtual ostream& put(ostream& s) const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  inline size_t serialSizeOf() const;
};
//______________________________________________________________________

/** Class allowing JigdoDesc to convey information back to the caller.
    The default versions of the methods do nothing at all (except for
    error(), which prints the error to cerr) - you need to supply an
    object of a derived class to functions to get called back. */
class JigdoDesc::ProgressReporter {
public:
  virtual ~ProgressReporter() { }
  /// General-purpose error reporting.
  virtual void error(const string& message);
  /// Like error(), but for purely informational messages.
  virtual void info(const string& message);
  /** Called when the output image (or a temporary file) is being
      written to. It holds that written==imgOff and
      totalToWrite==imgSize, *except* when additional files are merged
      into an already existing temporary file.
      @param written Number of bytes written so far
      @param totalToWrite Value of 'written' at end of write operation
      @param imgOff Current offset in image
      @param imgSize Total size of output image */
  virtual void writingImage(uint64 written, uint64 totalToWrite,
                            uint64 imgOff, uint64 imgSize);
};
//______________________________________________________________________

/** Container for JidoDesc objects. Is mostly a vector<JidoDesc*>, but
    deletes elements when destroyed. However, when removing elements,
    resizing the JigdoDescVec etc., the elements are *not* deleted
    automatically. */
class JigdoDescVec : public vector<JigdoDesc*> {
public:
  JigdoDescVec() : vector<JigdoDesc*>() { }
  inline ~JigdoDescVec();

  /** Read JigdoDescs from a template file into *this. *this is
      clear()ed first. File pointer must be at start of first entry;
      the "DESC" must have been read already. If error is thrown,
      position of file pointer is undefined. A type 1 (IMAGE_INFO)
      will end up at this->back(). */
  bistream& get(bistream& file) throw(JigdoDescError, bad_alloc);

  /** Write a DESC section to a binary stream. Note that there should
      not be two contiguous Unmatched regions - this is not checked.
      Similarly, the length of the ImageInfo part must match the
      accumulated lengths of the other parts. */
  bostream& put(bostream& file, MD5Sum* md = 0) const;

  /// List contents of a JigdoDescVec to a stream in human-readable format.
  void list(ostream& s) throw();
private:
  /// Disallow copying (too inefficient). Use swap() instead.
  inline JigdoDescVec& operator=(const JigdoDescVec&);
};

inline void swap(JigdoDescVec& x, JigdoDescVec& y) { x.swap(y); }

//======================================================================

JigdoDesc::ImageInfo::ImageInfo(uint64 s, const MD5& m, size_t b)
  : sizeVal(s), md5Val(m), blockLengthVal(b) { }
JigdoDesc::ImageInfo::ImageInfo(uint64 s, const MD5Sum& m, size_t b)
  : sizeVal(s), md5Val(m), blockLengthVal(b) { }

JigdoDesc::MatchedFile::MatchedFile(uint64 o, uint64 s, const RsyncSum64& r,
                                    const MD5& m)
  : offsetVal(o), sizeVal(s), rsyncVal(r), md5Val(m) { }
JigdoDesc::MatchedFile::MatchedFile(uint64 o, uint64 s, const RsyncSum64& r,
                                    const MD5Sum& m)
  : offsetVal(o), sizeVal(s), rsyncVal(r), md5Val(m) { }

//________________________________________

bool JigdoDesc::ImageInfo::operator==(const JigdoDesc& x) const {
  const ImageInfo* i = dynamic_cast<const ImageInfo*>(&x);
  if (i == 0) return false;
  else return size() == i->size() && md5() == i->md5();
}

bool JigdoDesc::UnmatchedData::operator==(const JigdoDesc& x) const {
  const UnmatchedData* u = dynamic_cast<const UnmatchedData*>(&x);
  if (u == 0) return false;
  else return size() == u->size();
}

bool JigdoDesc::MatchedFile::operator==(const JigdoDesc& x) const {
  const MatchedFile* m = dynamic_cast<const MatchedFile*>(&x);
  if (m == 0) return false;
  else return offset() == m->offset() && size() == m->size()
              && md5() == m->md5();
}

bool JigdoDesc::WrittenFile::operator==(const JigdoDesc& x) const {
  // NB MatchedFile and WrittenFile considered equal!
  const MatchedFile* m = dynamic_cast<const MatchedFile*>(&x);
  if (m == 0) return false;
  else return offset() == m->offset() && size() == m->size()
              && md5() == m->md5();
}
//________________________________________

inline bistream& operator>>(bistream& s, JigdoDescVec& v)
    throw(JigdoDescError, bad_alloc) {
  return v.get(s);
}

inline bostream& operator<<(bostream& s, JigdoDescVec& v) {
  return v.put(s);
}

JigdoDescVec::~JigdoDescVec() {
  for (iterator i = begin(), e = end(); i != e; ++i) delete *i;
}
//________________________________________

template<class Iterator>
Iterator JigdoDesc::ImageInfo::serialize(Iterator i) const {
  i = serialize1(IMAGE_INFO, i);
  i = serialize6(size(), i);
  i = ::serialize(md5(), i);
  i = serialize4(blockLength(), i);
  return i;
}
size_t JigdoDesc::ImageInfo::serialSizeOf() const { return 1 + 6 + 16 + 4; }

template<class Iterator>
Iterator JigdoDesc::UnmatchedData::serialize(Iterator i) const {
  i = serialize1(UNMATCHED_DATA, i);
  i = serialize6(size(), i);
  return i;
}
size_t JigdoDesc::UnmatchedData::serialSizeOf() const { return 1 + 6; }

template<class Iterator>
Iterator JigdoDesc::MatchedFile::serialize(Iterator i) const {
  i = serialize1(MATCHED_FILE, i);
  i = serialize6(size(), i);
  i = ::serialize(rsync(), i);
  i = ::serialize(md5(), i);
  return i;
}
size_t JigdoDesc::MatchedFile::serialSizeOf() const { return 1 + 6 + 8 + 16;}

template<class Iterator>
Iterator JigdoDesc::WrittenFile::serialize(Iterator i) const {
  i = serialize1(WRITTEN_FILE, i);
  i = serialize6(size(), i);
  i = ::serialize(rsync(), i);
  i = ::serialize(md5(), i);
  return i;
}
size_t JigdoDesc::WrittenFile::serialSizeOf() const { return 1 + 6 + 8 + 16;}

#endif

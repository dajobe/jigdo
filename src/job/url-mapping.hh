/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

*//** @file

  Representation of the directed, acyclic graph implied by the [Parts] and
  [Servers] lines in a .jigdo file. For each .jigdo file, we have one
  map<MD5, SmartPtr<PartUrlMapping> > which allows to create a set of URLs
  for each MD5 sum. Cf MakeImageDl::parts.

*/

#ifndef URL_MAPPING_HH
#define URL_MAPPING_HH

#ifndef INLINE
#  ifdef NOINLINE
#    define INLINE
#    else
#    define INLINE inline
#  endif
#endif

#include <config.h>

#include <string>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <debug.hh>
#include <md5sum.hh>
#include <nocopy.hh>
#include <smartptr.hh>
#include <status.hh>
#include <uri.hh> /* for findLabelColon() */
#include <url-mapping.fh>
//______________________________________________________________________

/** Object which represents a "Label:some/path" mapping, abstract base
    class. The "some/path" string is stored in the object, and the Label is
    represented with a pointer to another UrlMapping, whose output URL(s)
    need to be prepended to "some/path". UrlMappings can be chained into a
    linked list - that list is a list of alternative mappings for the same
    label. */
class UrlMapping : public SmartPtrBase, public NoCopy {
  friend class ServerUrlMapping;
  friend class PartUrlMapping;
public:
  /** For url-mapping-test: Do not init weight randomly. */
  static void setNoRandomInitialWeight();

  UrlMapping();
  virtual ~UrlMapping() = 0;

  /** Set value of URL part, starting with offset url[pos], up to n
      characters. */
  inline void setUrl(const string& url, string::size_type pos = 0,
                     string::size_type n = string::npos);
  /** Get URL value */
  inline const string& url() const { return urlVal; }

  /** Parse options from .jigdo file: --try-first[=..],
      --try-last[=..]. Unrecognized parameters are ignored, to make
      extensions of the .jigdo file format easier.  @return null on success,
      else error message. */
  const char* parseOptions(const vector<string>& value);

  /** If this UrlMapping is based on the string "Label:some/path" in a .jigdo
      file, then url()=="some/path" and prepend() points to the mapping(s)
      for "Label". */
  inline void setPrepend(UrlMapping* um) { prepVal = um; }
  inline UrlMapping* prepend() const { return prepVal.get(); }

  /** Insert a mapping into the singly linked list, it will be returned by
      the next call to next(). */
  inline void insertNext(UrlMapping* um);
  /** Return right peer of this object, or null. */
  inline UrlMapping* next() const { return nextVal.get(); }

  /** Return true iff url().empty() && prepend() == 0 */
  bool empty() const { return url().empty() && prepend() == 0; }

  /** Various knobs for the scoring algorithm */

  /** If two servers are rated equal by the scoring algorithm, the order in
      which the servers are tried should be random. Otherwise, if gazillions
      of people try to download the same thing using default settings (e.g.
      no country preference), the first server in its list shouldn't be hit
      too hard. In practice, we achieve randomisation by initializing the
      weight with a small random value, in the range
      [-RANDOM_INIT_RANGE,RANDOM_INIT_RANGE) */
  static const double RANDOM_INIT_RANGE;

private:
  string urlVal; // Part of URL
  SmartPtr<UrlMapping> prepVal; // URL(s) to prepend to this one, or null
  SmartPtr<UrlMapping> nextVal; // Alt. to this mapping; singly linked list
  //LineInJigdoFilePointer def; // Definition of this mapping in .jigdo file

  // Statistics, for server selection

  // Nr of times we tried to download an URL generated using this mapping
  //unsigned tries; // 0 or 1 for PartUrlMapping, more possible for servers
  // Number of above tries which failed (404 not found, checksum error etc)
  //unsigned triesFailed;

  /* Mapping-specific weight, includes user's global country preference,
     preference for this jigdo download's servers, global server preference.
     Does not change throughout the whole jigdo download. Can get <0. The
     higher the value, the higher the preference that will be given to this
     mapping. */
  double weight;
};
//______________________________________________________________________

/** If the .jigdo file contains [Servers] entries like "Foo=x" and "Foo=y",
    there will be one object for the "Foo=x" entry, its "next" pointer points
    to the "Foo=y" object.

    Special case: The .jigdo data will contain URLs starting with any of
    "http: ftp: https: ftps: gopher: file:", those protocol labels also get
    their own ServerUrlMapping objects. */
class ServerUrlMapping : public UrlMapping {
  // server-specific options: Supports resume, ...
  // server-specific availability counts
};
//______________________________________________________________________

/** Object to enumerate all URLs for "Label:some/path".  The order of
    preference for the enumeration can change dynamically, i.e. if the score
    of a server entry is decreased after several failed attempts to download
    from it, future weight calculations involving that server will take the
    decreased weight into account. */
class PartUrlMapping : public UrlMapping {
public:
  /** You must not call the addServer() method of this object's UrlMap after
      calling enumerate(). Otherwise, future results returned by this method
      will be complete nonsense. Enumerates all possible URLs, sorted by
      weight. Returns the empty string if all URLs enumerated. Internally,
      scans through the whole UrlMap each time, which can potentially take a
      long time. */
  string enumerate(vector<UrlMapping*>* best);

private:
  /* Because the UrlMapping data structure is not a tree, but an acyclic
     directed graph (i.e. tree with some branches coming together again),
     need to record the path through the structure while we recurse. */
  struct StackEntry {
    UrlMapping* mapping;
    StackEntry* up;
  };

  void enumerate(StackEntry* stackPtr, UrlMapping* mapping,
    double score, unsigned pathLen, unsigned* serialNr, double* bestScore,
    vector<UrlMapping*>* bestPath, unsigned* bestSerialNr);

  /* Set of URLs that were already returned by bestUnvisitedUrl(). Each URL
     is represented by a unique number, which is assigned to it by a
     depth-first scan of the tree-like structure in the UrlMap. */
  auto_ptr<set<unsigned> > seen;
};
//______________________________________________________________________

/** Object containing list of all Part and Server mappings in a .jigdo
    file */
class UrlMap : public NoCopy {
public:
  inline UrlMap();

  /** Add info about a mapping line inside one of the [Parts] sections in the
      .jigdo sections. The first entry of "value" is the URL (absolute,
      relative to baseUrl or in "Label:some/path form). The remaining "value"
      entries are assumed to be options.
      @return null if success, else error message */
  const char* addPart(const string& baseUrl, const MD5& md,
                      const vector<string>& value);

  /** Like addPart(), but intended for maintaining lists of PartUrlMapping
      objects where the checksum is not known. Used to maintain lists of URLs
      for .template files. While it does not alter the
      checksum=>PartUrlMapping mappings of this UrlMap, it /may/ alter the
      Label=>ServerUrlMapping mappings, and the returned/appended
      PartUrlMapping will reference the server mappings of this UrlMap.
      @param baseUrl Base URL, in case value.front() is a relative URL
      @param value value.front() is the URL, followed by args for the part
      @param oldList Pointer to 0 SmartPtr for first call, will create list
      head, or pointer to SmartPtr to list head, will then add to list. */
  const char* addPart(const string& baseUrl, const vector<string>& value,
                      SmartPtr<PartUrlMapping>* oldList);

  /** Add info about a [Servers] line, cf addPart(). For a line
      "Foobar=Label:some/path" in the [Servers] section:
      @param baseUrl Base URL, in case value.front() is a relative URL
      @param label == "Foobar"
      @param value arguments; value.front()=="Label:some/path"
      @return null if success, else error message */
  const char* addServer(const string& baseUrl, const string& label,
                        const vector<string>& value);


  /** Output the graph built up by addPart()/addServer() to the log. */
  void dumpJigdoInfo();

  /* [Parts] lines in .jigdo data; for each md5sum, there's a linked list of
     PartUrlMappings */
  typedef map<MD5, SmartPtr<PartUrlMapping> > PartMap;
  /* [Servers] lines in .jigdo data; for each label string, there's a linked
     list of ServerUrlMappings */
  typedef map<string, SmartPtr<ServerUrlMapping> > ServerMap;

  const PartMap& parts() const { return partsVal; }
  const ServerMap& servers() const { return serversVal; }

  /** Make lookups */
  inline PartUrlMapping* operator[](const MD5& m) const;

private:
  ServerUrlMapping* findOrCreateServerUrlMapping(const string& url,
                                                 unsigned colon);
  PartMap partsVal;
  ServerMap serversVal;
};
//======================================================================

void UrlMapping::insertNext(UrlMapping* um) {
  Paranoid(um != 0 && um->nextVal.isNull());
  um->nextVal = nextVal;
  nextVal = um;
}

void UrlMapping::setUrl(const string& url, string::size_type pos,
                        string::size_type n) {
  urlVal.assign(url, pos, n);
}

UrlMap::UrlMap() : partsVal(), serversVal() { }

PartUrlMapping* UrlMap::operator[](const MD5& m) const {
  PartUrlMapping* result;
  PartMap::const_iterator i = parts().find(m);
  if (i != parts().end()) result = i->second.get(); else result = 0;
  return result;
}

#endif

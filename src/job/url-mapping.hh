/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

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
#include <vector>

#include <debug.hh>
#include <md5sum.hh>
#include <nocopy.hh>
#include <smartptr.hh>
#include <status.hh>
#include <url-mapping.fh>
//______________________________________________________________________

/** Mapping md5sum => list of files */
class UrlMapping : public SmartPtrBase, public NoCopy {
public:
  /** url is overwritten! */
  UrlMapping();
  virtual ~UrlMapping() = 0;

  /** Set value of url part, starting with offset url[pos], up to n
      characters. */
  inline void setUrl(const string& url, string::size_type pos = 0,
                     string::size_type n = string::npos);
  /** Get url value */
  inline const string& url() const { return urlVal; }

  /** If this UrlMapping is based on the string "Label:some/path" in a .jigdo
      file, then url()=="some/path" and prepend() points to the mapping(s)
      for "Label". */
  inline void setPrepend(UrlMapping* um) { prepVal = um; }
  inline UrlMapping* prepend() const { return prepVal.get(); }

  /** Insert a mapping into the singly linked list, it will be returned by
      the next call to next(). */
  inline void insertNext(UrlMapping* um);
  inline UrlMapping* next() const { return nextVal.get(); }

  /** Various knobs for the scoring algorithm */

  /** If two servers are rated equal by the scoring algorithm, the order in
      which the servers are tried should be random. Otherwise, if gazillions
      of people try to download the same thing using default settings (e.g.
      no country preference), the first server in its list shouldn't be hit
      too hard. In practice, we achieve randomisation by initializing the
      weight with a small random value. */
  static const double RANDOM_INIT_LOWER = -.125;
  static const double RANDOM_INIT_UPPER = .125;

private:
  string urlVal; // Part of URL
  SmartPtr<UrlMapping> prepVal; // URL(s) to prepend to this one, or null
  SmartPtr<UrlMapping> nextVal; // Alt. to this mapping; singly linked list
  //LineInJigdoFilePointer def; // Definition of this mapping in .jigdo file

  // Statistics, for server selection

  // Nr of times we tried to download an URL generated using this mapping
  unsigned tries; // 0 or 1 for PartUrlMapping, more possible for servers
  // Number of above tries which failed (404 not found, checksum error etc)
  unsigned triesFailed;

  /* Mapping-specific weight, includes user's global country preference,
     preference for this jigdo download's servers, global server preference.
     Does not change throughout the whole jigdo download. Can get <0. The
     higher the value, the higher the preference that will be given to this
     mapping. */
  double weight;
};
//______________________________________________________________________

/* If the .jigdo file contains [Servers] entries like "Foo=x" and "Foo=y",
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

class PartUrlMapping : public UrlMapping {
  /* Recurse through list of mappings and find the mapping */
  //string getBestUrl();
};
//______________________________________________________________________

class UrlMap {
public:
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

  /* [Parts] lines in .jigdo data; for each md5sum, there's a linked list of
     PartUrlMappings */
  typedef map<MD5, SmartPtr<PartUrlMapping> > PartMap;
  /* [Servers] lines in .jigdo data; for each label string, there's a linked
     list of ServerUrlMappings */
  typedef map<string, SmartPtr<ServerUrlMapping> > ServerMap;

  const PartMap& parts() const { return partsVal; }
  const ServerMap& servers() const { return serversVal; }

private:
  ServerUrlMapping* findOrCreateServerUrlMapping(const string& url,
                                                 unsigned colon);

  PartMap partsVal;
  ServerMap serversVal;
};
//______________________________________________________________________

void UrlMapping::insertNext(UrlMapping* um) {
  Paranoid(um != 0 && um->nextVal.isNull());
  um->nextVal = nextVal;
  nextVal = um;
}

void UrlMapping::setUrl(const string& url, string::size_type pos,
                     string::size_type n) {
  urlVal.assign(url, pos, n);
}

#endif

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
#include <nocopy.hh>
#include <smartptr.hh>

#include <set> /* For multiset */

#include <debug.hh>
#include <url-mapping.fh>
//______________________________________________________________________

class UrlMapping : public SmartPtrBase, public NoCopy {
public:
  /** url is overwritten! */
  inline UrlMapping();
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
  //int weight;
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

UrlMapping::UrlMapping() : urlVal(), prepVal(0), nextVal(0), tries(0),
                           triesFailed(0)/*, weight(0)*/ { }

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

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download .jigdo/.template and file URLs
  Code for the directed acyclic graph of server and part mappings

*/

#include <config.h>

#include <log.hh>
#include <makeimagedl.hh>
#include <uri.hh>
#include <url-mapping.hh>
//______________________________________________________________________

using namespace Job;

DEBUG_UNIT("makeimagedl-graph")
//______________________________________________________________________

// map<MD5, SmartPtr<PartUrlMapping> > parts;

/* Given an URL-like string of the form "Label:some/path" or
   "http://foo/bar", return the ServerUrlMapping for "Label"/"http". If none
   exists, create one.
   @param url URL-like string
   @param colon Offset of ':' in url, must be >0
   @return new or existent mapping */
ServerUrlMapping* MakeImageDl::findOrCreateServerUrlMapping(
    const string& url, unsigned colon) {
  string label(url, 0, colon);
  ServerMap::iterator i = servers.lower_bound(label);
  if (i != servers.end() && i->first == label)
    return i->second.get(); // "Label" entry present, just return it

  // No entry for "Label" yet, need to create a dummy ServerUrlMapping
  ServerUrlMapping* s = new ServerUrlMapping();
  /* Initialize the url for label "http" with "http:"; addServer() below will
     recognize this special case. */
  SmartPtr<ServerUrlMapping> ss(s);
  servers.insert(i, make_pair(label, ss));
  label += ':';
  s->setUrl(label);
  return s;
}
//____________________

void MakeImageDl::addPart(const string& baseUrl, const MD5& md,
                          vector<string>& value) {
  string url;
  uriJoin(&url, baseUrl, value.front());
  debug("addPart %1 -> %2", md.toString(), url);

  PartUrlMapping* p = new PartUrlMapping();
  unsigned colon = findLabelColon(url);
  if (colon == 0) {
    p->setUrl(url);
  } else {
    p->setPrepend(findOrCreateServerUrlMapping(url, colon));
    p->setUrl(url, colon + 1);
  }
  // Insert entry in "parts"
  SmartPtr<PartUrlMapping> pp(p);
  pair<PartMap::iterator, bool> x =
    parts.insert(make_pair(md, pp));
  Paranoid(x.first->first == md);
  if (!x.second) {
    // entry for md already present in parts, add p to its linked list
    x.first->second->insertNext(p);
  }
}
//____________________

/* For a line "Foobar=Label:some/path" in the [Servers] section:
   @param label == "Foobar"
   @param value arguments; value.front()=="Label:some/path" */
Status MakeImageDl::addServer(const string& baseUrl, const string& label,
                              vector<string>& value) {
  string url;
  uriJoin(&url, baseUrl, value.front());
  debug("addServer %1 -> %2", label, url);

  /* Create entry for "Foobar". We usually create a new ServerUrlMapping,
     except in the case where findOrCreateServerUrlMapping() has created a
     dummy entry during previous processing of a [Parts] section. */
  ServerUrlMapping* s;
  ServerUrlMapping* mappingList; // Ptr to head of linked list for label
  ServerMap::iterator i = servers.lower_bound(label);
  if (i == servers.end() || i->first != label) {
    // Create object and start a new linked list; add list head to "servers"
    s = mappingList = new ServerUrlMapping();
    SmartPtr<ServerUrlMapping> ss(s);
    servers.insert(i, make_pair(label, ss));
  } else {
    const string& somepath = i->second->url();
    if (!somepath.empty() && somepath[somepath.length() - 1] == ':') {
      // List head is dummy; use it directly
      s = mappingList = i->second.get();
    } else {
      // Create object and add it to existing linked list
      mappingList = i->second.get();
      s = new ServerUrlMapping();
      i->second->insertNext(s);
    }
  }

  /* Process the "Label:some/path" string, maybe adding a dummy
     ServerUrlMapping for "Label". */
  unsigned colon = findLabelColon(url);
  if (colon == 0) {
    s->setUrl(url);
  } else {
    ServerUrlMapping* prep = findOrCreateServerUrlMapping(url, colon);
    s->setPrepend(prep);
    s->setUrl(url, colon + 1);
    // Check whether this is a recursive definition
    UrlMapping* i = prep;
    do {
      if (i == mappingList) { // Cycle detected
        // Break cycle, leave s in nonsensical state. Maybe also delete prep
        s->setPrepend(0);
        return FAILED;
      }
      i = i->prepend();
    } while (i != 0);
  }
  return OK;
}
//______________________________________________________________________

void MakeImageDl::dumpJigdoInfo() {
  // Build reverse map from UrlMapping* to its label
  map<UrlMapping*, string> names;
  for (ServerMap::iterator i = servers.begin(), e = servers.end();
       i != e; ++i) {
    names.insert(make_pair(i->second.get(), i->first));
  }

  for (PartMap::iterator i = parts.begin(), e = parts.end(); i != e; ++i) {
    UrlMapping* p = i->second.get();
    while (p != 0) {
      debug("Part %1: %2 + `%3'",
            i->first.toString(), names[p->prepend()], p->url());
      p = p->next();
    }
  }

  for (ServerMap::iterator i = servers.begin(), e = servers.end();
       i != e; ++i) {
    UrlMapping* p = i->second.get();
    while (p != 0) {
      debug("Server %1: %2 + `%3'",
            i->first, names[p->prepend()], p->url());
      p = p->next();
    }
  }
}

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

  Representation of a "<md5sum>=Label:someurl" line in a .jigdo file

*/

#include <config.h>

#include <glib.h>
#include <float.h>

#include <compat.hh>
#include <debug.hh>
#include <log.hh>
#include <uri.hh>
#include <url-mapping.hh>
//______________________________________________________________________

DEBUG_UNIT("url-mapping")

namespace {

  struct RandSingleton {
    GRand* r;
    RandSingleton() : r(g_rand_new()) { }
    ~RandSingleton() { g_rand_free(r); }
  };
  RandSingleton r;

}

const double UrlMapping::RANDOM_INIT_RANGE = 0.03125;

namespace { bool randomInit = true; }
void UrlMapping::setNoRandomInitialWeight() { randomInit = false; }

UrlMapping::UrlMapping()
  : urlVal(), prepVal(0), nextVal(0)/*, tries(0), triesFailed(0)*/ {
  if (randomInit)
    weight = g_rand_double_range(r.r, -RANDOM_INIT_RANGE, RANDOM_INIT_RANGE);
  else
    weight = 0.0;
}

UrlMapping::~UrlMapping() { }
//______________________________________________________________________

// map<MD5, SmartPtr<PartUrlMapping> > parts;

/* Given an URL-like string of the form "Label:some/path" or
   "http://foo/bar", return the ServerUrlMapping for "Label"/"http". If none
   exists, create one.
   @param url URL-like string
   @param colon Offset of ':' in url, must be >0
   @return new or existent mapping */
ServerUrlMapping* UrlMap::findOrCreateServerUrlMapping(
    const string& url, unsigned colon) {
  string label(url, 0, colon);
  ServerMap::iterator i = serversVal.lower_bound(label);
  if (i != serversVal.end() && i->first == label)
    return i->second.get(); // "Label" entry present, just return it

  // No entry for "Label" yet, need to create a dummy ServerUrlMapping
  ServerUrlMapping* s = new ServerUrlMapping();
  /* Initialize the url for label "http" with "http:"; addServer() below will
     recognize this special case. */
  SmartPtr<ServerUrlMapping> ss(s);
  serversVal.insert(i, make_pair(label, ss));
  label += ':';
  s->setUrl(label);
  return s;
}
//____________________

const char* UrlMap::addPart(const string& baseUrl, const MD5& md,
                            const vector<string>& value) {
  string url;
  if (findLabelColon(value.front()) != 0)
    url = value.front();
  else
    uriJoin(&url, baseUrl, value.front());
  //debug("addPart %1 -> %2", md.toString(), url);

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
    partsVal.insert(make_pair(md, pp));
  Paranoid(x.first->first == md);
  if (!x.second) {
    // entry for md already present in partsVal, add p to its linked list
    x.first->second->insertNext(p);
  }
  return p->parseOptions(value);
}
//____________________

const char* UrlMap::addPart(const string& baseUrl, const vector<string>& value,
                            SmartPtr<PartUrlMapping>* oldList) {
  Assert(oldList != 0);
  string url;
  if (findLabelColon(value.front()) != 0)
    url = value.front();
  else
    uriJoin(&url, baseUrl, value.front());

  PartUrlMapping* p = new PartUrlMapping();
  unsigned colon = findLabelColon(url);
  if (colon == 0) {
    p->setUrl(url);
  } else {
    p->setPrepend(findOrCreateServerUrlMapping(url, colon));
    p->setUrl(url, colon + 1);
  }
  if (*oldList == 0)
    *oldList = p;
  else
    (*oldList)->insertNext(p);
  return p->parseOptions(value);
}
//____________________

/* For a line "Foobar=Label:some/path" in the [Servers] section:
   @param label == "Foobar"
   @param value arguments; value.front()=="Label:some/path" */
const char* UrlMap::addServer(const string& baseUrl, const string& label,
                              const vector<string>& value) {
  string url;
  if (findLabelColon(value.front()) != 0)
    url = value.front();
  else
    uriJoin(&url, baseUrl, value.front());
  debug("addServer %1 -> %2", label, url);

  /* Create entry for "Foobar". We usually create a new ServerUrlMapping,
     except in the case where findOrCreateServerUrlMapping() has created a
     dummy entry during previous processing of a [Parts] section. */
  ServerUrlMapping* s;
  ServerUrlMapping* mappingList; // Ptr to head of linked list for label
  ServerMap::iterator i = serversVal.lower_bound(label);
  if (i == serversVal.end() || i->first != label) {
    // Create object and start a new linked list; add list head to "servers"
    s = mappingList = new ServerUrlMapping();
    SmartPtr<ServerUrlMapping> ss(s);
    serversVal.insert(i, make_pair(label, ss));
  } else {
    const string& somepath = i->second->url();
    // Dummy mapping recognizable by trailing ':'
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
        return _("Recursive label definition");
      }
      i = i->prepend();
    } while (i != 0);
  }
  return s->parseOptions(value);
}
//______________________________________________________________________

void UrlMap::dumpJigdoInfo() {
  // Build reverse map from UrlMapping* to its label
  map<UrlMapping*, string> names;
  for (ServerMap::iterator i = serversVal.begin(), e = serversVal.end();
       i != e; ++i) {
    names.insert(make_pair(i->second.get(), i->first));
  }

  for (PartMap::iterator i = partsVal.begin(), e = partsVal.end();
       i != e; ++i) {
    UrlMapping* p = i->second.get();
    while (p != 0) {
      debug("Part %1: %2 + `%3'",
            i->first.toString(), names[p->prepend()], p->url());
      p = p->next();
    }
  }

  for (ServerMap::iterator i = serversVal.begin(), e = serversVal.end();
       i != e; ++i) {
    UrlMapping* p = i->second.get();
    while (p != 0) {
      debug("Server %1: %2 + `%3'",
            i->first, names[p->prepend()], p->url());
      p = p->next();
    }
  }
}
//______________________________________________________________________

const char* UrlMapping::parseOptions(const vector<string>& value) {
  for (vector<string>::const_iterator i = value.begin(), e = value.end();
       i != e; ++i) {
    if (compat_compare(*i, 0, 11, "--try-first", 0, 11) == 0) {
      if (i->length() == 11) {
        weight += 1.0;
      } else if ((*i)[11] == '=') {
        double d = 0.0;
        if (sscanf(i->c_str() + 12, "%lf", &d) != 1)
          return _("Invalid argument to --try-first");
        weight += d;
      }
    } else if (compat_compare(*i, 0, 10, "--try-last", 0, 10) == 0) {
      if (i->length() == 10) {
        weight -= 1.0;
      } else if ((*i)[10] == '=') {
        double d = 0.0;
        if (sscanf(i->c_str() + 11, "%lf", &d) != 1)
          return _("Invalid argument to --try-last");
        weight -= d;
      }
    }
  }
  return 0;
}
//______________________________________________________________________

string PartUrlMapping::enumerate(vector<UrlMapping*>* bestPath) {
  if (seen.get() == 0)
    seen.reset(new set<unsigned>());

  string result;
  double bestScore = -FLT_MAX;
  unsigned serialNr = 0;
  unsigned bestSerialNr = 0;

  bestPath->clear();
  UrlMapping* mapping = this;
  do {
    //debug("enumerate: at top-level: %1", mapping->url());
    enumerate(0, mapping, 0.0, 0, &serialNr, &bestScore,
              bestPath, &bestSerialNr); // Recurse
    mapping = mapping->next(); // Walk through list of peers
  } while (mapping != 0);

  if (bestSerialNr != 0) {
    seen->insert(bestSerialNr); // Ensure this URL is only output once
    for (vector<UrlMapping*>::iterator i = bestPath->begin(),
           e = bestPath->end(); i != e; ++i)
      result += (*i)->url(); // Construct URL
    debug("enumerate: \"%1\" with score %2", result, bestScore);
  } else {
    debug("enumerate: end");
  }
  return result;
}

/* @param stackPtr For recording how we reached this "mapping".
   @param mapping Current node in graph
   @param score Accumulated scores of objects through which we came here
   @param pathLen Nr of objects through which we reached "mapping"
   @param serialNr Nr of leaves encountered so far during recursion. One leaf
   may be reached through >1 paths in the graph; in that case, it counts >1
   times.
   @param bestScore Highest score found so far
   @param bestPath Path through graph corresponding to bestScore
   @param bestSerialNr Value of serialNr for this leaf obj
*/
void PartUrlMapping::enumerate(StackEntry* stackPtr, UrlMapping* mapping,
    double score, unsigned pathLen, unsigned* serialNr, double* bestScore,
    vector<UrlMapping*>* bestPath, unsigned* bestSerialNr) {
//   debug("enumerate: pathLen=%1 serialNr=%2 url=%3", pathLen, *serialNr,
//         mapping->url());
  // Update score to include "mapping" object
  score += mapping->weight;
  ++pathLen;

  if (mapping->prepend() == 0) {
    /* Landed at leaf of acyclic graph, i.e. mapping has no further "Label:"
       prepended to it. (In practice, mapping->url() is "http:" or
       "ftp:".). Check whether this path's score is a new maximum. */
    //debug("enumerate: Leaf");
    ++*serialNr;
    if (*serialNr == 0) {
      --*serialNr; return; // Whoa, overflow! Should Not Happen(tm)
    }
    // Score of path = SUM(scores_of_path_elements) / length_of_path
    double pathScore = score / implicit_cast<double>(pathLen);
    if (pathScore > *bestScore
        && seen->find(*serialNr) == seen->end()) {
      debug("enumerate: New best score %1", pathScore);
      // New best score found
      *bestScore = pathScore;
      *bestSerialNr = *serialNr;
      bestPath->clear();
      bestPath->push_back(mapping);
      StackEntry* s = stackPtr;
      while (s != 0) {
        bestPath->push_back(s->mapping);
        s = s->up;
      }
    }
    return;
  }

  // Not at leaf object - continue recursion
  StackEntry stack;
  stack.mapping = mapping;
  stack.up = stackPtr;
  mapping = mapping->prepend(); // Descend
  do {
    enumerate(&stack, mapping, score, pathLen, serialNr, bestScore,
              bestPath, bestSerialNr); // Recurse
    mapping = mapping->next(); // Walk through list of peers
  } while (mapping != 0);
}

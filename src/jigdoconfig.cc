/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Representation for config data in a .jigdo file - based on ConfigFile

*/

#include <errno.h>
#include <iostream>
#include <fstream>
#include <string.h>

#include <config.h>
#include <jigdoconfig.hh>
#include <log.hh>
#include <string.hh>
//______________________________________________________________________

DEBUG_UNIT("jigdoconfig")

void JigdoConfig::ProgressReporter::error(const string& message) {
  cerr << message << endl;
}
void JigdoConfig::ProgressReporter::info(const string& message) {
  cerr << message << endl;
}
//________________________________________

namespace {
  void forwardPrep(string& result, const string& fileName,
                   size_t lineNr, const string& message) {
    result = subst("%1:%2: %3", fileName, static_cast<uint64>(lineNr),
                   message);
  }
}

void JigdoConfig::ForwardReporter::error(const string& message,
                                         const size_t lineNr) {
  string s;
  forwardPrep(s, fileName, lineNr, message);
  reporter->error(s);
}
void JigdoConfig::ForwardReporter::info(const string& message,
                                        const size_t lineNr) {
  string s;
  forwardPrep(s, fileName, lineNr, message);
  reporter->info(s);
}
//______________________________________________________________________

JigdoConfig::JigdoConfig(const char* jigdoFile, ProgressReporter& pr)
    : config(0), serverMap(), freporter(pr, jigdoFile) {
  ifstream f(jigdoFile);
  if (!f) {
    string err = subst(_("Could not open `%1' for input: %2"),
                       jigdoFile, (errno != 0 ? strerror(errno) : ""));
    freporter.reporter->error(err);
    return;
  }

  // Read config data
  config = new ConfigFile(freporter);
  f >> *config;

  rescan();
# if DEBUG
  debug("[Servers] mapping is:");
  for (Map::iterator i = serverMap.begin(), e =serverMap.end(); i != e; ++i){
    for (vector<string>::iterator j = i->second.begin(), k = i->second.end();
         j != k; ++j)
      debug("    %1 => %2", i->first, *j);
  }
# endif
}
//______________________________________________________________________

JigdoConfig::JigdoConfig(const char* jigdoFile, ConfigFile* configFile,
                         ProgressReporter& pr)
    : config(configFile), serverMap(), freporter(pr, jigdoFile) {
  configFile->setReporter(freporter);
  rescan();
}

JigdoConfig::JigdoConfig(const string& jigdoFile, ConfigFile* configFile,
                         ProgressReporter& pr)
    : config(configFile), serverMap(), freporter(pr, jigdoFile) {
  configFile->setReporter(freporter);
  rescan();
}
//______________________________________________________________________

namespace {
  const char* const SECTION_NAME = "Servers";
}

/* Creates mapping from label name to URI. Syntactically incorrect
   lines are just ignored here, the file is assumed to be correct.
   Error in case of loops, e.g. "LabelA=LabelB:foo/",
   "LabelB=LabelA:bar". */
void JigdoConfig::rescan() {
  ConfigFile::iterator first = config->firstSection(SECTION_NAME);
  ConfigFile::iterator i = first;
  ConfigFile::iterator end = config->end();
  list<ServerLine> entries;

  serverMap.clear();

  /* Build a list of all the entries inside [Servers] sections in the
     file, to ease their subsequent handling. */
  while (i != end) {
    ServerLine l;
    // For each "Label=..." line inside a "[Servers]" section
    while (i.nextLabel()) { // Go to next non-comment, non-empty line
      if (!i.setLabelOffsets(l.labelStart, l.labelEnd, l.valueStart))
        continue;
      l.line = i;
      entries.push_back(l);
    }
    // Go to next "[Servers]" section
    --i; // because the first thing nextSection() does is advance i
    i.nextSection(SECTION_NAME);
  }

  // Set up serverMap
  bool printError = true;
  while (!entries.empty()) {
    ServerLine& l = entries.front();
    string label(*l.line, l.labelStart, l.labelEnd - l.labelStart);
    debug("rescan: `%1'", label);
    rescan_addLabel(entries, label, printError);
  }
}
//________________________________________

void JigdoConfig::rescan_makeSubst(list<ServerLine>& entries,
    Map::iterator mapl, const ServerLine& l, bool& printError) {
  // Split the value, "Foo:some/path", into whitespace-separated words
  vector<string> words;
  ConfigFile::split(words, l.line, l.valueStart);
# if 0
  if (words.size() > 1 && printError) {
    /* In the future, there might be support for --switches, so don't
       allow >1 words per URI. */
    string err = subst(_("Line contains more than one URI, ignoring part "
                         "after `%1' (maybe you need to use \"\" quotes?)"),
                       words[0]);
    size_t lineNr = 1;
    ConfigFile::iterator findLineNr = l.line;
    while (findLineNr != config->begin()) { ++lineNr; --findLineNr; }
    freporter.error(err, lineNr);
    printError = false;
  }
# endif
  //____________________

  // Where to append new URIs for this label
  vector<string>& uris = mapl->second;

  // Extract label reference ("Foo") from value

  if (words.empty()) {
    // Empty value, e.g. for *l.line=="MD5Sum=", so add empty string
    uris.push_back("");
    return;
  }

  string& s = words.front();
  string::size_type colon = s.find(':');
  if (colon == string::npos) {
    debug("Appending to entry for `%1' fixed mapping `%2'", mapl->first, s);
    uris.push_back(s); // No label ref, append directly
    return;
  }
  //____________________

  string refLabel(s, 0, colon);
  debug("rescan_addLabel   ref `%1'", refLabel);

  // Ensure there's a map entry for refLabel, even if it's empty - RECURSE
  Map::iterator mapr = rescan_addLabel(entries, refLabel, printError);
  Paranoid(mapr != serverMap.end());
  if (mapr->second.empty()) {
    debug("Appending to entry for `%1' fixed mapping `%2'", mapl->first, s);
    uris.push_back(s); // Label not defined, so no substitution
    return;
  }

  /* For each of the mappings of "Foo", substitute the "Foo:" in
     "label" with the mapping */
  debug("Appending to entry for `%1' mappings for `%2':",
        mapl->first, refLabel);
  for (vector<string>::iterator i = mapr->second.begin(),
         e = mapr->second.end(); i != e; ++i) {
    uris.push_back(*i);
    uris.back().append(s, colon + 1, string::npos);
    debug("    %1", uris.back());
  }
  return;
}
//________________________________________

/* Add to serverMap a mapping for a line "Label=Foo:some/path" in the
   config file. "Label" is the key for serverMap, and this method adds
   entries to the corresponding value_type (which is a
   vector<string>).

   If the value of the line begins with a reference to another label,
   e.g. "Foo" for the value "Foo:some/path", recursively try to find a
   list of absolute URIs that the reference expands to. If that list
   is empty (this will be the case for labels like "http", "ftp"!),
   append the value unchanged to the vector, otherwise append one
   string to the vector for each URI in the list, which is formed by
   replacing the label reference "Foo:" with the URI.

   Infinite recursion due to a loop of labels referencing each other
   is not possible, because we create an empty entry *before*
   recursing. NB, in case of loops, the resulting mapping is
   undefined; in practice, all the mappings "below" what caused the
   loop will be omitted, not only the members of the loop themselves.

   first is just a (small) efficiency improvement; it points to the
   first "Label=Foo:some/path" in the config file. */

JigdoConfig::Map::iterator JigdoConfig::rescan_addLabel(
    list<ServerLine>& entries, const string& label, bool& printError) {
  Map::iterator mapl = serverMap.find(label);
  if (mapl != serverMap.end()) return mapl;

  // Create entry in serverMap for this label.
  debug("rescan_addLabel label `%1'", label);
  pair<Map::iterator,bool> x =
      serverMap.insert(make_pair(label, vector<string>()));
  mapl = x.first;

  // Search through the list for entries with label.
  while (true) {
    list<ServerLine>::iterator i = entries.begin(), e = entries.end();
    // <sigh> - 2 different libstdc++ versions, not one single way for this:
    //while (i != e && label.compare(*(i->line), i->labelStart,
    //                               i->labelEnd - i->labelStart) != 0) ++i;
    //while (i != e
    //  && label.compare(0, string::npos,
    //     *(i->line), i->labelStart, i->labelEnd - i->labelStart) != 0) ++i;
    // So do it manually - advance i until "label" matches "label{Start,End}"
    while (i != e) {
      size_t labelLen = i->labelEnd - i->labelStart;
      if (label.size() == labelLen) {
        size_t j = 0;
        while (j < labelLen && label[j] == (*i->line)[i->labelStart + j])
          ++j;
        if (j == labelLen) break; // Found
      }
      ++i;
    }

    if (i == e) break;
    /* Entry found - add it to mapl.second, substituting any "Foo:"
       references with the possible values for "Foo". */
    rescan_makeSubst(entries, mapl, *i, printError);
    // Remove entry
    entries.erase(i);
  }
  return mapl;
}

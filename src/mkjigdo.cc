/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Create location list (.jigdo) and image template (.template)

  This is the part of the mktemplate code concerned with creating the .jigdo
  file. See mktemplate.cc for the .template code and the main run() method.

*/

#include <config.h>

#include <string>
#include <vector>

#include <jigdoconfig.hh>
#include <md5sum.hh>
#include <mimestream.hh>
#include <mktemplate.hh>
#include <scan.hh>
#include <string.hh>
//______________________________________________________________________

namespace {
  /* On entry, i points to a line. Move it upwards >= 0 lines,
     skipping any comment lines immediately before the line, but not
     any empty lines. */
  void skipPrevComments(ConfigFile::iterator& i) {
    string::const_iterator x;
    do {
      --i;
      x = i->begin();
      ConfigFile::advanceWhitespace(x, i->end());
    } while (x != i->end() && *x == '#');
    ++i;
  }
}

struct MkTemplate::PartIndex {
  bool operator()(const PartLine* a, const PartLine* b) const {
    return compat_compare(a->text, a->split, string::npos, b->text,
                          b->split, string::npos) < 0;
  }
};
//______________________________

/* Set up some sections/entries in jigdo file. jigdo->configFile() is
   either empty or contains a jigdo file passed to the program with
   --merge. Is called before the MkTemplate operation does its main
   work. */
void MkTemplate::prepareJigdo() {
  ConfigFile& j = jigdo->configFile();
  typedef ConfigFile::iterator iterator;

  /* Remove existing [Parts] section(s) from file and store the
     entries in jigdoParts. During template generation, new entries
     are added to the set. At the end, the parts are output again. The
     reason why we don't simply append to an existing [Parts] section
     is that in the final jigdo file, the parts should always come
     after all other sections, so that if the download tool features
     progressive display, the list of images can be displayed soon. */
  matchedParts.clear();
  jigdoParts.clear();
  string sect = "Parts";
  iterator curr = j.firstSection(sect);
  while (curr != j.end()) {
    iterator prev = curr; ++curr; j.erase(prev); // Remove [Parts] line
    // Walk through lines of this [Parts] section
    while (curr != j.end() && !curr.isSection()) {
      string::const_iterator s = curr->begin();
      ConfigFile::advanceWhitespace(s, curr->end());
      prev = curr; ++curr;
      if (s == prev->end()) {
        j.erase(prev); // Remove empty lines
      } else if (*s != '#') { // Leave alone comment lines
        // Remove entry lines, enter them into jigdoParts
        size_t begin, end, value; // value is offset of first char after '='
        PartLine x;
        if (prev.setLabelOffsets(begin, end, value)) {
          x.text.assign(*prev, value, string::npos);
          x.split = x.text.length();
          x.text.append(*prev, begin, end - begin);
        } else {
          x.text.swap(*prev);
          x.split = x.text.length();
        }
        jigdoParts.insert(x);
        j.erase(prev);
      }
    }
    // Advance to next [Parts] section, if any
    if (curr == j.end()) break;
    if (curr.isSection(sect)) continue;
    curr.nextSection(sect);
  }
  j.rescan();
  //____________________

  // Create [Jigdo] section, unless already present
  if (j.firstSection("Jigdo") == j.end()) {
    // If jigdo file empty, insert small header comment
    if (j.empty()) {
      j.push_back("# JigsawDownload");
      j.push_back("# See <");
      j.back() += URL;
      j.back() += "> for details about jigdo";
      j.push_back();
    }
    iterator i = j.firstSection();
    skipPrevComments(i);
    j.insert(i, "[Jigdo]");
    j.insert(i, "Version=");
    iterator x = i; --x;
    append(*x, FILEFORMAT_MAJOR); *x += '.'; append(*x, FILEFORMAT_MINOR);
    j.insert(i, "Generator=jigdo-file/" JIGDO_VERSION);
    j.insert(i);
  }
  j.rescan();
}
//______________________________________________________________________

/* After MkTemplate has read the image, add [Parts] and [Image]
   sections to the jigdo. */
void MkTemplate::finalizeJigdo(const string& imageLeafName,
                               const string& templLeafName,
                               const MD5Sum& templMd5Sum) {
  ConfigFile& j = jigdo->configFile();
  typedef ConfigFile::iterator iterator;

  // Add new [Image] section or add Template-MD5Sum value to existing section
  if (addImageSection) {
    Base64String md5Sum;
    md5Sum.write(templMd5Sum.digest(), 16).flush();

    // Search for first empty "Template-MD5Sum=" line in file
    size_t off = 0;
    string sect = "Image";
    string label = "Template-MD5Sum";
    for (ConfigFile::Find f(&j, sect, label, &off);
         !f.finished(); off = f.next()) {
      // Is the value empty, i.e. end of line after the '='?
      string::iterator x = f.label()->begin() + off;
      if (ConfigFile::advanceWhitespace(x, f.label()->end())) {
        /* Append md5sum to existing Template-MD5Sum line - more
           accurately, insert md5sum before any # comment. */
        f.label()->insert(off, md5Sum.result());
        break;
      }
    }
    if (off == 0) {
      // Append a new [Image] section
      if (!j.back().empty()) j.push_back();
      j.push_back("[Image]");
      //j.rescan();
      j.push_back("Filename=");
      j.back() += imageLeafName;
      j.push_back("Template=");
      j.back() += templLeafName;
      j.push_back("Template-MD5Sum=");
      j.back() += md5Sum.result();
      j.rescan();
    }
  }
  //____________________

  /* Prepare to add server lines to last [Servers] section in file. If
     no such section present yet, create one. */
  ConfigFile::iterator servers = j.end();
  if (addServersSection) {
    string sect = "Servers";
    iterator serverSection = j.firstSection(sect);
    while (serverSection != j.end()) {
      servers = serverSection;
      serverSection.nextSection(sect);
    }
    if (servers != j.end()) {
      servers.nextSection();
      servers.prevLabel();
      ++servers;
    } else {
      Paranoid(!j.empty());
      if (!j.back().empty()) j.push_back();
      j.push_back("[Servers]");
      j.rescan();
    }
  }
  //____________________

  /* Add new lines to [Parts] section, but only if the part's md5sum
     isn't listed in the section yet. */
  {
    // Build index over JigdoParts by md5sum string in partIndex
    set<const PartLine*,PartIndex> partIndex;
    for (set<PartLine>::iterator i = jigdoParts.begin(),
           e = jigdoParts.end(); i != e; ++i)
      partIndex.insert(&*i);

    // For quick check: Has LocationPath already been added to [Servers]?
    set<const LocationPath*> locPaths;
    // Auto-generated label name
    string locName = "A";
    string sect = "Servers";

    // Now add FileParts from matchedParts to jigdoParts as PartLines
    Base64String m;
    for (vector<FilePart*>::iterator i = matchedParts.begin(),
           e = matchedParts.end(); i != e; ++i) {
      m.write((*i)->getMD5Sum(cache)->digest(), 16).flush();
      PartLine x;
      x.text.swap(m.result());
      x.split = 0;
      Paranoid(m.result().empty());
      // Skip if md5sum already in partIndex
      if (partIndex.find(&x) != partIndex.end()) continue;
      // Otherwise, add a new entry
      LocationPathSet::iterator location = (*i)->getLocation();
      string s = location->getLabel();
      if (s.empty()) { // Need to create new label name
        while (true) {
          ConfigFile::Find f(&j, sect, locName);
          if (f.finished()) break;
          size_t n = 0;
          while (++locName[n] == 'Z' + 1) { // "Increment locName"
            locName[n] = 'A';
            if (++n == locName.size()) { locName.append(1, 'A'); break; }
          }
        }
        const_cast<LocationPath&>(*location).setLabel(locName);
        s = locName;
      }
      s += ':';
      s += (*i)->leafName();
      s = ConfigFile::quote(s);
      x.split = s.length();
      s += x.text;
      s.swap(x.text);
      pair<set<PartLine>::iterator,bool> partIns = jigdoParts.insert(x);
      partIndex.insert(&*partIns.first);

      // Maybe also add a line to the [Servers] section
      if (!addServersSection) continue;
      pair<set<const LocationPath*>::iterator,bool> locIns =
        locPaths.insert(&*location);
      if (!locIns.second) continue; // Already been seen - ignore
      s = location->getLabel();
      s += '=';
      s += location->getUri();
      if (j.find(sect, s) == j.end()) j.insert(servers, s);
    }
  }
  matchedParts.clear();

  // Add [Parts] section
  if (!j.back().empty()) j.push_back();
  j.push_back("[Parts]");
  j.rescan();
  for (set<PartLine>::iterator i = jigdoParts.begin(), e = jigdoParts.end();
       i != e; ++i) {
    j.push_back();
    j.back().append(i->text, i->split, string::npos);
    j.back() += '=';
    j.back().append(i->text, 0, i->split);
  }
  jigdoParts.clear();
}

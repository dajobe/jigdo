/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Access to Gnome/KDE/ini-style configuration files

  Quite simple, inefficient implementation - but more flexible WRT
  updating the config file before writing it back to disc.

*/

#include <config.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <configfile.hh>
#include <string.hh>
//______________________________________________________________________

void ConfigFile::ProgressReporter::error(const string& message,
                                         const size_t lineNr) {
  if (lineNr > 0) cerr << lineNr << ": ";
  cerr << message << endl;
}
void ConfigFile::ProgressReporter::info(const string& message,
                                        const size_t lineNr) {
  if (lineNr > 0) cerr << lineNr << ": ";
  cerr << message << endl;
}

ConfigFile::ProgressReporter ConfigFile::noReport;
//______________________________________________________________________

ConfigFile::~ConfigFile() {
  Line* l = endElem.next;
  while (l != &endElem) {
    Line* tmp = l;
    l = l->next;
    delete tmp;
  }
}
//______________________________________________________________________

/* Update the linked list formed by the nextSect members after the
   list of sections in the file has been modified. This must be called
   by the user, it cannot be handled fully automatically (with a flag
   "needs rescan"), because the user could assign "[newsection]"
   anytime to any of the strings accessible via iterators. */
void ConfigFile::rescan(bool printErrors) {
  firstSect() = &endElem;
  Line** previous = &firstSect();
  size_t thisLine = 0; // Current line number
  bool inComment = false; // true if currently inside a [Comment] section

  for (iterator i = begin(), e = end(); i != e; ++i) {
    ++thisLine;
    //cerr << "   " << thisLine << ' ' << *i << endl;
    // Insert into linked list any "[sectionname]" lines
    string::const_iterator x = i->begin();
    string::const_iterator end = i->end();
    i.nextSect() = 0;
    // Empty line, or only contains '#' comment
    if (advanceWhitespace(x, end)) continue;
    if (*x != '[') {
      if (inComment || !printErrors) continue;
      // If printErrors, do extra syntax check for label name
      while (true) {
        if (x == end) {
          reporter->error(_("Label name is not followed by `='"), thisLine);
          break;
        }
        if (*x == '=') break;
        if (*x == '[' || *x == ']' || *x == '#') {
          string err = subst(_("Label name contains invalid character `%1'"),
                             *x);
          reporter->error(err, thisLine);
          break;
        }
        ++x;
      }
      continue; // Next line
    }

    if (printErrors) {
      /* Scan label name. At the same time, check whether the name is
         "Comment" or "comment", ignore the whole section if it is. */
      ++x; // Advance beyond the '['
      if (advanceWhitespace(x, end)) { // Skip space after '['
        reporter->error(_("No closing `]' for section name"), thisLine);
        continue;
      }
      string::const_iterator s1 = x; // s1 points to start of section name
      while (x != end && *x != ']' && !isWhitespace(*x) && *x != '['
             && *x != '=' && *x != '#') ++x;
      string::const_iterator s2 = x; // s2 points to end of section name
      if (advanceWhitespace(x, end)) {
        reporter->error(_("No closing `]' for section name"), thisLine);
        continue;
      }
      // In special case of "Include", format differs: URL after section name
      if (s2 - s1 == 7
          && strncmp(i->c_str() + (s1 - i->begin()), "Include", 7) == 0) {
        while (x != end && *x != ']') ++x; // Skip URL
      }
      if (*x != ']') {
        string err = subst(_("Section name invalid at character `%1'"),
                           *x);
        reporter->error(err, thisLine);
        continue;
      }
      ++x; // Advance beyond the ']'
      if (!advanceWhitespace(x, end)) {
        reporter->error(_("Invalid characters after closing `]'"), thisLine);
        continue;
      }
      // Check for "comment"/"Comment"
      if (s2 - s1 == 7 && (*s1 == 'C' || *s1 == 'c')) {
        string::const_iterator s = s1;
        static const char* const omment = "comment";
        int i = 1;
        do { if (*++s != omment[i]) break; ++i; } while (i < 7);
        inComment = (i == 7);
      }
    }

    // Line is a [section] line
    // Prepare for insertion into list of [section] lines
    *previous = i.p;
    previous = &(i.nextSect());
  }
  *previous = &endElem; // Close ring
  lineCount = thisLine;
}
//______________________________________________________________________

ConfigFile::iterator ConfigFile::find(const string& sectName,
                                      const string& line) {
  iterator i = firstSection(sectName);
  while (i != end()) {
    ++i;
    while (!i.isSection()) { // &&!i.isEnd() unnecessary
      if (*i == line) return i;
      ++i;
    }
    if (i.p->isEnd()) return i;
    if (!i.isSection(sectName)) i.nextSection(sectName);
  }
  return i;
}
//______________________________________________________________________

istream& ConfigFile::get(istream& s) {
  string text;
  while (true) {
    getline(s, text);
    // Tolerate Doze "CRLF"-style line endings under Unix
    if (text[text.length() - 1] == '\r') text.resize(text.length() - 1);
    if (!s) break;
    push_back();
    swap(text, back());
    Paranoid(text.empty());
  }
  rescan(true);
  return s;
}
//______________________________________________________________________

ostream& ConfigFile::put(ostream& s) const {
  // Too lazy to implement all the const_iterator stuff, so cast const away
  ConfigFile* self = const_cast<ConfigFile*>(this);
  for (iterator i = self->begin(), e = self->end(); i != e; ++i)
    s << *i << '\n';
  return s;
}
//______________________________________________________________________

bool ConfigFile::iterator::isSection(const string& sectName) const {
  if (!isSection()) return false;

  // Skip whitespace at start of line
  const string::const_iterator send = sectName.end();
  string::const_iterator x = p->text.begin();
  string::const_iterator xend = p->text.end();
  bool emptyLine = advanceWhitespace(x, xend);
  Assert(!emptyLine);
  Assert(*x == '[');
  ++x;
  bool emptyAfterOpeningBracket = advanceWhitespace(x, xend);
  Assert(!emptyAfterOpeningBracket);

  // Compare section names
  string::const_iterator s = sectName.begin();
  while (x != xend && s != send && *x == *s) { ++x; ++s; }
  if (s == send // End of sectName; now only ']' and whitespace may follow
      && !advanceWhitespace(x, xend) // advance til ']'
      && *x == ']') { // check for ']'
    ++x;
    bool extraCharsAfterClosingBracket = advanceWhitespace(x, xend);
    Assert(extraCharsAfterClosingBracket);
    return true; // Found matching [section] line
  }
  return false;
}
//______________________________________________________________________

ConfigFile::iterator& ConfigFile::iterator::nextSection(
    const string& sectName) {
  // Advance to next [section] line
  nextSection();

  // Look for matching section name
  while (!p->text.empty()) { // i.e. while end() not reached
    if (isSection(sectName)) return *this;
    // No match, advance to next [section] line
    nextSection();
  }

  // End of file reached, nothing found
  return *this;
}
//______________________________________________________________________

size_t ConfigFile::iterator::nextLabel(const string& labelName) {
  string::const_iterator x;
  string::const_iterator xend;
  const string::const_iterator lend = labelName.end();
  while (true) {
    // Advance to next line
    ++*this;
    if (isSection()) return 0; // End of section or end of file
    // Next line is superfluous cos endElem.isSection() == true
    //if (p->isEnd()) return 0;

    x = p->text.begin();
    xend = p->text.end();
    // Skip whitespace at start of line
    if (advanceWhitespace(x, xend)) continue; // Skip empty lines

    // Compare label names
    string::const_iterator l = labelName.begin();
    while (x != xend && l != lend && *x == *l) { ++x; ++l; }
    if (l == lend // End of labelName; now only '=' may follow
        && !advanceWhitespace(x, xend) // advance til '='
        && *x == '=') // check for '='
      return x - p->text.begin() + 1; // Found matching label= line
  }
}
//______________________________________________________________________

/* No full syntax check; e.g. label names may (incorrectly) contain
   '['. NB an empty label name is allowed. */
bool ConfigFile::iterator::setLabelOffsets(size_t& begin, size_t& end,
                                           size_t& value) {
  if (isSection()) return false;
  string::const_iterator xbeg = p->text.begin();
  string::const_iterator xend = p->text.end();
  string::const_iterator x = xbeg;
  if (advanceWhitespace(x, xend)) return false;
  begin = x - xbeg;
  end = begin;
  // Search forward to '='
  while (x != xend) {
    if (*x == '=') {
      value = x - xbeg + 1;
      return true;
    }
    // When calculating end offset, ignore whitespace before the '='
    if (*x != ' ' && *x != '\t') end = x - xbeg + 1;
    ++x;
  }
  return false;
}
//______________________________________________________________________

ConfigFile::Find::Find(ConfigFile* c, const string& sectName,
                       const string& labelName, const iterator i,
                       size_t* offset)
    : configFile(c), sectionStr(sectName), labelStr(labelName),
      sectionIter(i), rightSection(false) {
  --sectionIter;
  labelIter = sectionIter;
  // Special-case for implicit 0th section with empty section name
  if (labelIter == configFile->end() && sectionStr.empty())
    rightSection = true;
  size_t o = next();
  if (offset != 0) *offset = o;
}

ConfigFile::Find::Find(ConfigFile* c, const string& sectName,
                       const string& labelName, size_t* offset)
    : configFile(c), sectionStr(sectName), labelStr(labelName),
      sectionIter(c->begin()), rightSection(false) {
  --sectionIter;
  labelIter = sectionIter;
  // Special-case for implicit 0th section with empty section name
  if (labelIter == configFile->end() && sectionStr.empty())
    rightSection = true;
  size_t o = next();
  if (offset != 0) *offset = o;
}
//______________________________________________________________________

/* The Find implementation uses the knowledge that for ConfigFile
   objects, --begin() == end() and also ++--begin() == begin().

   Note that we do not strictly /need/ the additional "sectionIter"
   data member, but it is necessary for users to detect whether the
   search has jumped over from one section to the next (identically
   named) section, and to efficiently restart the search in the same
   section. */

size_t ConfigFile::Find::next() {
  while (true) {
    /* If the line pointed to by section() is not named sectName,
       advance both section() and label() to the next section of that
       name. */
    if (!rightSection) {
      if (sectionIter.nextSection(sectionStr) == configFile->end()) {
        // No more sections found
        labelIter = configFile->end();
        return 0;
      }
      labelIter = sectionIter;
      rightSection = true;
    }

    /* Next, advance label() to the next label called labelName.
       Repeat the process with further sections if no label of that
       name in this section. */
    size_t o = labelIter.nextLabel(labelStr);
    if (o > 0 // Success, label found
        || labelIter == configFile->end()) // End of search, no more matches
      return o;

    // End of section - look for more sections with given name
    rightSection = false;
  }
}
//______________________________________________________________________

// Returns true if s has been set to a valid word (which have length zero)
bool ConfigFile::split1Word(string* word, const string& s,
                            string::const_iterator& e) {
  string::const_iterator end = s.end();
  enum {
    none,    // Outside any quotes
    dbl,     // Inside double quotes
    single   // Inside single quotes
  } state = none;

  // Skip whitespace
  while (e != end && (*e == ' ' || *e == '\t')) ++e;
  if (e == end || *e == '#') return false;

  do {
    if (*e == '"') {
      // Double quotes, enter/leave "" mode
      switch (state) {
      case none: state = dbl; break;
      case dbl: state = none; break;
      case single: *word += '"'; break;
      }
    } else if (*e == '\'') {
      // Single quotes, enter/leave '' mode
      switch (state) {
      case none: state = single; break;
      case dbl: *word += '\''; break;
      case single: state = none; break;
      }
    } else if (*e == ' ' || *e == '\t' || *e == '#') {
      // Whitespace - end word?
      if (state == none) return true;
      *word += *e;
    } else if (*e == '\\') {
      if (state != single) {
        ++e;
        if (e == end) return true;
      }
      *word += *e;
    } else {
      *word += *e;
    }
    ++e;
  } while (e != end);
  return true;
}
//______________________________________________________________________

string& ConfigFile::quote(string& s) {
  bool singleQuote = false;
  bool needQuotes = false; // Need to enclose s either in '' or ""
  size_t escNeeded = 0; // Nr of \ to insert for "" quoting
  for (string::const_iterator i = s.begin(), e = s.end(); i != e; ++i) {
    if (*i == '\'') singleQuote = true;
    else if (*i == ' ' || *i == '\t' || *i == '#') needQuotes = true;
    else if (*i == '"' || *i == '\\') ++escNeeded;
  }
  if (!singleQuote && !needQuotes && escNeeded == 0) // No quoting necessary
    return s;
  if (!singleQuote) { // Can enclose string in single quotes
    s.insert(s.begin(), '\'');
    s += '\'';
    return s;
  }
  // Must use double quotes and escape with \ where necessary
  ++escNeeded;
  s.insert(s.begin(), escNeeded, '"'); // Make enough room at start of s
  string::iterator out = s.begin() + 1;
  for (string::iterator i = s.begin()+escNeeded, e = s.end(); i != e; ++i) {
    if (*i == '"' || *i == '\\') { *out = '\\'; ++out; }
    *out = *i; ++out;
  }
  Paranoid(out == s.end());
  s += '"';
  return s;
}

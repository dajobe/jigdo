/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Access to Gnome/KDE/ini-style configuration files

  Allow reading and writing of configuration files. The files consist
  of a number of sections, introduced with "[SectionName]" on a line
  by itself. Within each section, there are entries of the form
  "Label=value". Example for a .jigdo file:
  ________________________________________

  [Jigdo]
  Version=1.0
  Generator=jigdo-file/0.5.1

  # Comment
  [Image] # Comment
  Filename=image
  Template=http://edit.this.url/image.template
  ShortInfo=This is a CD image
  Info=Some more info about the image.
   Whee, this entry extends over more than one line!
  .
   It even contains an empty line, above this one.
  # ^^^^^^ multi-line values UNIMPLEMENTED at the moment

  [Parts]
  QrxELOWvjQ2JgkFhlkT74w=A:ironmaiden/part88
  jKVYd3dxh68ROwI6NSQxGA=A:ironmaiden/part87
  ________________________________________

  Whitespace is removed at the start of lines, to the left of the "="
  in an entry line and at the start and end of a section name, but
  nowhere else. This means that a section name or label may contain
  spaces, possibly even multiple consecutive spaces. Section and label
  names cannot contain the characters []=#

  Entries appearing before the first section name are added to a 0th
  section named "" (empty string). Empty label names are also allowed.

  Comments are introduced with "#" and extend to the end of the line.
  They may appear after a "[SectionName]" or on a separate line, but
  not in entry lines (if they do, they're considered part of the
  entry's value).

  Furthermore, multi-line comments are possible because a section
  named [Comment] or [comment] is treated specially; errors about
  incorrect entries are not reported. In practice, this means that any
  text can be written in these sections, as long as no line begins
  with '['.

  Searches for sections/labels are case-sensitive.

  Multi-line entry values are not implemented ATM.

*/

#ifndef CONFIGFILE_HH
#define CONFIGFILE_HH

#include <iosfwd>
#include <list>
#include <string>

#include <debug.hh>
//______________________________________________________________________

/** General approach: Reading/changes/writing of config should be
    possible, and all formatting and comments made by any human
    editing the file should be preserved. Consequently, a ConfigFile
    behaves like a list<string> simply containing the raw data as read
    from the file. Access is possible via a subset of the list<>
    methods, or higher-level methods to find sections/entries.

    NB: Changing/writing to disc of config not currently supported. */
class ConfigFile {
public:
  class ProgressReporter;
  class iterator;
  inline ConfigFile(ProgressReporter& pr = noReport);
  ~ConfigFile();

  /** This must be called after *sections* have been added/removed/
      renamed, to update the list of sections present in the config
      file. No need to call it after insertion/deletion of lines,
      whitespace/comment changes of [section] lines, or any changes to
      entries.
      @printErrors If true, perform extra syntax checks and call
      ProgressReporter object for syntax errors. */
  void rescan(bool printErrors = false);

  /** Change reporter for error messages */
  void setReporter(ProgressReporter& pr) { reporter = &pr; }

  /** Input from file, append to this. Makes a call to rescan(true). */
  istream& get(istream& s);
  /** Output to file */
  ostream& put(ostream& s) const;
  //______________________________

  /** Return iterator to first real [section] line in the file */
  inline iterator firstSection() { return iterator(*firstSect()); }
  /** Return iterator to first section with given name, or to end() */
  inline iterator firstSection(const string& sectName);

  /** Standard list interface */
  typedef string& reference;
  typedef const string& const_reference;

  size_t size() const { return lineCount; }
  bool empty() const { return size() == 0; }

  inline iterator begin();
  //inline const_iterator begin() const;
  inline iterator end();
  //inline const_iterator end() const;
  inline reference front();
  //inline const_reference front() const;
  inline reference back();
  //inline const_reference back() const;

  // Return iterator to first such line in section, or end()
  iterator find(const string& sectName, const string& line);

  // Insert empty line before pos
  inline iterator insert(iterator pos);
  // Insert s before pos
  inline iterator insert(iterator pos, const_reference s);
  inline iterator insert(iterator pos, const char* s);
  // Delete line at pos from list; pos becomes invalid
  inline iterator erase(iterator pos);
  //inline iterator erase(iterator first, iterator last);
  // Add line at end of list
  inline void push_back(); // Empty line
  inline void push_back(const string& s);
  inline void push_back(const char* s);
  //______________________________

  /** Helper function: Advance x until it points to end or a
      non-space, non-tab character. Returns true if at end of string
      (or '#' comment). */
  static inline bool advanceWhitespace(string::const_iterator& x,
                                       const string::const_iterator& end);
  static inline bool advanceWhitespace(string::iterator& x,
                                       const string::const_iterator& end);
  /// Is character a space or tab?
  static inline bool isWhitespace(char x) { return x == ' ' || x == '\t'; }

  /** Helper function, useful to post-process entry values: Given an
      iterator and an offset in the string pointed to, extract the
      entry value (everything starting with the specified offset) and
      break it into whitespace-separated words, which are appended to
      out. This does many things that a shell does:
      - Allow quoting with "" or ''. Whitespace between quotes does
        not cause the word to be split there.
      - Except inside '', escaping double quote, space, # or backslash
        with \ is possible.
      - A comment can be added at the end of the line.
      Escapes like \012, \xff, \n, \t are *not* supported, behaviour
      is undefined. (Possible future extension, TODO: Allow \ at end
      of line for multi-line entries?) */
  template<class Container> // E.g. vector<string>; anything with push_back()
  static void split(Container& out, const iterator i, size_t offset = 0);
  /** Related to above; if necessary, modifies s in such a way that it
      stays one word: If it contains whitespace, " or \ then enclose
      it in '' and if it contains ' then enclose it in "" and
      additionally escape other problematical characters with \.
      Returns reference to s. */
  static string& quote(string& s);

private:
  struct Line {
    Line() : prev(), next(), nextSect(0), text() { }
    Line(const string& s) : prev(), next(), nextSect(0), text(s) { }
    Line(const char* s) : prev(), next(), nextSect(0), text(s) { }
    // Returns true if *this is the end() element
    bool isEnd() const { return nextSect != 0 && text.empty(); }
    Line* prev;
    Line* next;
    // Linked ring of [section] lines, or null for non-section lines
    Line* nextSect;
    string text;
  };
  ProgressReporter* reporter;
  // Disallow copying - copy ctor is never defined
  inline ConfigFile(const ConfigFile&);
  /* Dummy element for end(). Its "next" ptr points to the first, its
     "prev" ptr to the last element, i.e. doubly linked ring of Line
     objects. Its nextSect is ptr to linked ring of [section] lines,
     or ptr to endElem itself if no sections present. */
  Line endElem;
  size_t lineCount;
  Line*& firstSect() { return endElem.nextSect; }

  /// Non-template helper function for split() that does the actual work
  static bool split1Word(string& s, const iterator& i,
                         string::const_iterator& e);

  /// Default reporter: Only prints error messages to stderr
  static ProgressReporter noReport;

public:
  //______________________________

  /** The iterators hide the fact that a ConfigFile is not a
      list<string>. */
  class iterator {
    friend class ConfigFile;
  public:
    iterator() { }
    iterator(const iterator& i) : p(i.p) { }
    iterator& operator=(const iterator& i) { p = i.p; return *this; }
    // Default dtor
    reference operator*() const { return p->text; }
    reference operator*() { return p->text; }
    string* operator->() const { return &p->text; }
    string* operator->() { return &p->text; }
    iterator& operator++() { p = p->next; return *this; }
    iterator& operator--() { p = p->prev; return *this; }
    bool operator==(const iterator i) const { return p == i.p; }
    bool operator!=(const iterator i) const { return p != i.p; }
    /// Is this line a [section] line?
    bool isSection() const { return p->nextSect != 0; }
    /// Is this line a [section] line with the given name?
    bool isSection(const string& sectName) const;
    /** Advance iterator to next [section] line. Efficient only if
        current line is also a [section] line - otherwise, does linear
        search. Results in *this == end() if no more sections. Does
        not look at the string; only relies on the info created during
        rescan(). */
    inline iterator& nextSection();
    /** Advance iterator to next [section] line with given section
        name. Assumes that rescan() has been called, if it hasn't then
        assertions will fail. sectName should be a correct section
        name, i.e. no whitespace at start or end. */
    iterator& nextSection(const string& sectName);
    /** Advance iterator to next non-empty, non-comment line. If it is
        a label line, return true. Otherwise (a [section] line, or at
        end()), return false. */
    inline bool nextLabel();
    /// Advance to previous non-empty, non-comment line
    inline bool prevLabel();
    /** Advance iterator to next label line with given label name, or
        to next [section] line, whichever comes first. Results in
        *this == end() if no more sections and label not found.
        Returns 0 if unsuccessful, i.e. iterator points to [section]
        line or to end(). Otherwise, returns offset to the entry's
        value; offset in line of first character after '='. labelName
        must not start or end with spaces. */
    size_t nextLabel(const string& labelName);
    /** Overwrite arguments with offset of first character of label
        name, offset of first character after label name, and offset
        of first character after the '='. Returns false if this is not
        a label line. */
    bool setLabelOffsets(size_t& begin, size_t& end, size_t& value);
  private:
    iterator(Line& l) : p(&l) { }
    Line*& nextSect() { return p->nextSect; }
    Line* p;
  };
  //______________________________

  /** Class to enumerate all lines in the config file which match a
      given section & label name. NB: Only references are maintained
      to the section/label name, but it is assumed that these strings
      remain unchanged as long as the Find object is used. WARNING
      WARNING this means that you *must*not* pass string constants to
      the Find ctor, only strings - the lifetime of the temporary
      strings created from string constants is usually too short!!!
      Usage:

      for (ConfigFile::Find f(c, sectionStr, labelStr);
           !f.finished(); f.next()) {
        // f.section() points to "[section]" line, or end() if 0th section
        // f.label()   points to "label=..." line, or end() if f.finished()
      }

      size_t off;
      for (ConfigFile::Find f(c, sectionStr, labelStr, &off);
           !f.finished(); off = f.next()) {
        // f.section() points to "[section]" line, or end() if 0th section
        // f.label()   points to "label=..." line, or end() if f.finished()
        // off is offset of part after "label=", or 0
      }
  */
  class Find {
  public:
    // Default copy ctor, dtor
    /** @param i where to start searching. If i points to a [section]
        line, the search will start there, if it doesn't, the search
        will start beginning with the next section after i. If you
        search for the implicit section with an empty name (for labels
        before the first [section] line in the file), you _must_
        supply sectName=="" and i==begin() (or omit i to use the
        second form).
        @param offset If non-null, is overwritten with 0
        (unsuccessful) or the offset in the line to the character
        after '=', i.e. the offset to the entry's value. */
    Find(ConfigFile* c, const string& sectName, const string& labelName,
         const iterator i, size_t* offset = 0);
    Find(ConfigFile* c, const string& sectName, const string& labelName,
         size_t* offset = 0);
    /** If the line pointed to by section() is not named sectName,
        advance both section() and label() to the next section of that
        name. Next, advance label() to the next label called
        labelName. Repeat the process with further sections if no
        label of that name in this section.
        @param offset If non-null, is overwritten with 0
        (unsuccessful) or the offset in the line to the character
        after '=', i.e. the offset to the entry's value. */
    size_t next();
    /// Current section line. NB takes a copy, can't change the Find object
    iterator section() const { return sectionIter; }
    /// Current line with label
    iterator label() const { return labelIter; }
    /// Returns true if finished enumerating
    bool finished() const { return labelIter == configFile->end(); }
  private:
    ConfigFile* configFile;
    const string& sectionStr;
    const string& labelStr;
    iterator sectionIter; // For section line
    iterator labelIter; // For label line
    bool rightSection;
  };
  // TODO: Find_const, which works on a const ConfigFile...
};
//______________________________________________________________________

/** Class allowing ConfigFile to convey information back to the
    creator of a ConfigFile object. */
class ConfigFile::ProgressReporter {
public:
  virtual ~ProgressReporter() { }
  /** General-purpose error reporting. lineNr==0 is a special case for
      "don't report any line number" */
  virtual void error(const string& message, const size_t lineNr = 0);
  /** Like error(), but for purely informational messages. Default
      implementation, just like that of error(), prints message to
      cerr */
  virtual void info(const string& message, const size_t lineNr = 0);
};
//______________________________________________________________________

ConfigFile::ConfigFile(ProgressReporter& pr)
    : reporter(&pr), endElem(), lineCount(0) {
  endElem.prev = &endElem;
  endElem.next = &endElem;
  endElem.nextSect = &endElem;
}

bool ConfigFile::advanceWhitespace(string::const_iterator& x,
                                   const string::const_iterator& end) {
  while (true) {
    if (x == end || *x == '#') return true;
    if (*x != ' ' && *x != '\t') return false;
    ++x;
  }
}
bool ConfigFile::advanceWhitespace(string::iterator& x,
                                   const string::const_iterator& end) {
  while (true) {
    if (x == end || *x == '#') return true;
    if (*x != ' ' && *x != '\t') return false;
    ++x;
  }
}

ConfigFile::iterator& ConfigFile::iterator::nextSection() {
  // p automatically ends up pointing to end()
  if (isSection()) {
    p = p->nextSect;
  } else {
    do p = p->next; while (!isSection());
  }
  return *this;
}

bool ConfigFile::iterator::nextLabel() {
  while (true) {
    ++*this;
    if (isSection()) return false;
    // Next line is superfluous cos endElem.isSection() == true
    //if (p->isEnd()) return false;

    /* Skip any empty line. If there is *any* character on this line,
       it must be a label line; it cannot be a section line because
       isSection() == false above */
    string::const_iterator x = p->text.begin();
    if (!advanceWhitespace(x, p->text.end())) return true;
  }
}

bool ConfigFile::iterator::prevLabel() {
  while (true) {
    --*this;
    if (isSection()) return false;
    string::const_iterator x = p->text.begin();
    if (!advanceWhitespace(x, p->text.end())) return true;
  }
}

ConfigFile::iterator ConfigFile::firstSection(const string& sectName) {
  iterator i = end();
  i.nextSection(sectName);
  return i;
}

ConfigFile::iterator ConfigFile::begin() { return iterator(*endElem.next); }
ConfigFile::iterator ConfigFile::end() { return iterator(endElem); }
ConfigFile::reference ConfigFile::front() { return endElem.next->text; }
ConfigFile::reference ConfigFile::back() { return endElem.prev->text; }

ConfigFile::iterator ConfigFile::insert(iterator pos) {
  Line* x = new Line();
  x->prev = pos.p->prev; x->next = pos.p;
  pos.p->prev->next = x; pos.p->prev = x;
  ++lineCount;
  return pos;
}
ConfigFile::iterator ConfigFile::insert(iterator pos, const_reference s) {
  Line* x = new Line(s);
  x->prev = pos.p->prev; x->next = pos.p;
  pos.p->prev->next = x; pos.p->prev = x;
  ++lineCount;
  return pos;
}
ConfigFile::iterator ConfigFile::insert(iterator pos, const char* s) {
  Line* x = new Line(s);
  x->prev = pos.p->prev; x->next = pos.p;
  pos.p->prev->next = x; pos.p->prev = x;
  ++lineCount;
  return pos;
}
ConfigFile::iterator ConfigFile::erase(iterator pos) {
  Paranoid(pos.p != 0); // Don't erase an element twice
  Paranoid(!pos.p->isEnd()); // Don't c.erase(c.end())
  pos.p->next->prev = pos.p->prev;
  pos.p->prev->next = pos.p->next;
  --lineCount;
  delete pos.p;
  pos.p = 0;
  return pos;
}
void ConfigFile::push_back() {
  insert(iterator(endElem));
}
void ConfigFile::push_back(const string& s) {
  insert(iterator(endElem));
  back() = s;
}
void ConfigFile::push_back(const char* s) {
  insert(iterator(endElem));
  back() = s;
}
//______________________________________________________________________

inline ostream& operator<<(ostream& s, const ConfigFile& c) {
  return c.put(s);
}
inline istream& operator>>(istream& s, ConfigFile& c) {
  return c.get(s);
}
//______________________________________________________________________

template<class Container>
void ConfigFile::split(Container& out, const iterator i, size_t offset) {
  string s;
  string::const_iterator e = i->begin() + offset;
  while (split1Word(s, i, e)) {
    out.push_back(string());
    swap(s, out.back());
  }
}

#endif

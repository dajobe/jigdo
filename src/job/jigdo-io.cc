/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  IO object for .jigdo downloads; download, gunzip, interpret

*/

#include <config.h>

#include <configfile.hh>
#include <debug.hh>
#include <jigdo-io.hh>
#include <log.hh>
#include <makeimagedl.hh>
#include <md5sum.hh>
//______________________________________________________________________

DEBUG_UNIT("jigdo-io")

using namespace Job;

namespace {

  inline bool isWhitespace(char x) { return ConfigFile::isWhitespace(x); }

  inline bool advanceWhitespace(string::const_iterator& x,
                                const string::const_iterator& end) {
    return ConfigFile::advanceWhitespace(x, end);
  }

  inline bool advanceWhitespace(string::iterator& x,
                                const string::const_iterator& end) {
    return ConfigFile::advanceWhitespace(x, end);
  }
}

JigdoIO::JigdoIO(MakeImageDl::Child* c, const string& url,
                 DataSource::IO* frontendIo)
  : childDl(c), urlVal(url), frontend(frontendIo), parent(0), includeLine(0),
    firstChild(0), next(0), rootAndImageSectionCandidate(0), line(0),
    section(), imageSectionLine(0), imageName(), imageInfo(),
    imageShortInfo(), templateMd5(0), childFailedId(0), gunzip(this) { }

JigdoIO::JigdoIO(MakeImageDl::Child* c, const string& url,
                 DataSource::IO* frontendIo, JigdoIO* parentJigdo,
                 unsigned inclLine)
  : childDl(c), urlVal(url), frontend(frontendIo), parent(parentJigdo),
    includeLine(inclLine), firstChild(0), next(0),
    rootAndImageSectionCandidate(0), line(0), section(), imageSectionLine(0),
    imageName(), imageInfo(), imageShortInfo(), templateMd5(0),
    childFailedId(0), gunzip(this) { }
//______________________________________________________________________

JigdoIO::~JigdoIO() {
  debug("~JigdoIO");

  if (childFailedId != 0) {
    Assert(false); // Situation untested
    g_source_remove(childFailedId);
    childFailedId = 0;
    master()->childFailed(childDl, this, frontend);
  }

  // Delete all our children
  JigdoIO* x = firstChild;
  while (x != 0) {
    JigdoIO* y = x->next;
    delete x;
    x = y;
  }
}
//______________________________________________________________________

Job::IO* JigdoIO::job_removeIo(Job::IO* rmIo) {
  debug("job_removeIo %1", rmIo);
  if (rmIo == this) {
    // Should never be called for jigdo
    Assert(false);
    master()->childFailed(childDl, this, frontend);
    Job::IO* c = frontend;
    // Do not "delete this" - childDl owns us and the SingleUrl
    return c;
  } else if (frontend != 0) {
    Job::IO* c = frontend->job_removeIo(rmIo);
    Paranoid(c == 0 || dynamic_cast<DataSource::IO*>(c) != 0);
    debug("job_removeIo frontend=%1", c);
    frontend = static_cast<DataSource::IO*>(c);
  }
  return this;
}

void JigdoIO::job_deleted() {
  if (frontend != 0) frontend->job_deleted();
  // Do not "delete this" - childDl owns us
}

void JigdoIO::job_succeeded() {
  if (failed()) return;
  if (frontend != 0) frontend->job_succeeded();
  master()->childSucceeded(childDl, this, frontend);
}

void JigdoIO::job_failed(string* message) {
  Paranoid(!failed());
  if (failed()) return;
  if (frontend != 0) frontend->job_failed(message);
  string err = _("Download of .jigdo file failed");
  master()->generateError(&err);
  master()->childFailed(childDl, this, frontend);
}

void JigdoIO::job_message(string* message) {
  if (frontend != 0) frontend->job_message(message);
}

void JigdoIO::dataSource_dataSize(uint64 n) {
  if (frontend != 0) frontend->dataSource_dataSize(n);
}

void JigdoIO::dataSource_data(const byte* data, size_t size,
                              uint64 currentSize) {
  if (master()->finalState()) return;
  Assert(master()->state() == MakeImageDl::DOWNLOADING_JIGDO);
  debug("Got %1 bytes", size);
  try {
    gunzip.inject(data, size);
  } catch (Error e) {
    ++line;
    generateError(e.message.c_str());
    return;
  }
  if (frontend != 0) frontend->dataSource_data(data, size, currentSize);
}
//______________________________________________________________________

void JigdoIO::gunzip_deleted() { }

void JigdoIO::gunzip_needOut(Gunzip*) {
  /* This is only called once, at the very start - afterwards, we always call
     setOut() from gunzip_data, so Gunzip won't call this. */
  gunzip.setOut(gunzipBuf, GUNZIP_BUF_SIZE);
}

/* Uncompressed data arrives. "decompressed" points somewhere inside
   gunzipBuf. Split data apart at \n and interpret line(s), then copy any
   remaining unfinished line to the start of gunzipBuf. The first byte of
   gunzipBuf (if it contains valid data) is always the first char of a line
   in the config file. */
void JigdoIO::gunzip_data(Gunzip*, byte* decompressed, unsigned size) {
//   // If an error happened earlier, ignore this call to gunzip_data()
//   if (gunzipBuf == 0) return;
  if (failed()) return;

  // Look for end of line.
  byte* p = decompressed;
  const byte* end = decompressed + size;
  const byte* stringStart = gunzipBuf;
  string line;

  while (p < end) {
    if (*p == '\n') {
      // Add new line to ConfigFile
      Paranoid(static_cast<unsigned>(p - stringStart) <= GUNZIP_BUF_SIZE);
      Paranoid(line.empty());
      const char* lineChars = reinterpret_cast<const char*>(stringStart);
      if (g_utf8_validate(lineChars, p - stringStart, NULL) != TRUE)
        throw Error(_("Input .jigdo data is not valid UTF-8"));
      line.append(lineChars, p - stringStart);
      jigdoLine(&line);
      ++p;
      stringStart = p;
      continue;
    }
    if (*p == '\r')
      *p = ' '; // Allow Windows-style line endings by turning CR into space
    else if (*p == 127 || (*p < 32 && *p != '\t')) // Check for evil chars
     throw Error(_("Input .jigdo data contains invalid control characters"));
    ++p;
  }

  if (stringStart == gunzipBuf && p == stringStart + GUNZIP_BUF_SIZE) {
    // A single line fills the whole buffer. Truncate it at that length.
    debug("gunzip_data: long line");
    Paranoid(line.empty());
    const char* lineChars = reinterpret_cast<const char*>(stringStart);
    if (g_utf8_validate(lineChars, p - stringStart, NULL) != TRUE)
      throw Error(_("Input .jigdo data is not valid UTF-8"));
    line.append(lineChars, p - stringStart);
    jigdoLine(&line);
    // Trick: To ignore remainder of huge line, prepend a comment char '#'
    gunzipBuf[0] = '#';
    gunzip.setOut(gunzipBuf + 1, GUNZIP_BUF_SIZE - 1);
    return;
  }

  unsigned len = p - stringStart;
  if (len > 0 && stringStart > gunzipBuf) {
    // Unprocessed data left somewhere inside the buffer - copy to buf start
    Assert(len < GUNZIP_BUF_SIZE); // Room must be left in the buffer
    memmove(gunzipBuf, stringStart, len);
  }
  gunzip.setOut(gunzipBuf + len, GUNZIP_BUF_SIZE - len);
}

void JigdoIO::gunzip_failed(string* message) {
  throw Error(*message, true);
}
//______________________________________________________________________

void JigdoIO::generateError(const char* msg) {
  string err;
  err = subst(_("%1 (line %2 in %3)"), msg, line,
              (source() != 0 ? source()->location().c_str() : "?") );
  debug("generateError: %1", err);
  Paranoid(!failed());
  if (failed()) return;
  if (frontend != 0) frontend->job_failed(&err);
  err = _("Error processing .jigdo file contents");
  master()->generateError(&err);

  /* We cannot call this right now:
     master()->childFailed(childDl, this, frontend);
     so schedule a callback to call it later. */
  childFailedId = g_idle_add_full(G_PRIORITY_HIGH_IDLE,&childFailed_callback,
                                  (gpointer)this, NULL);
  Paranoid(childFailedId != 0);
  Paranoid(failed());
}

gboolean JigdoIO::childFailed_callback(gpointer data) {
  JigdoIO* self = static_cast<JigdoIO*>(data);
  debug("childFailed_callback for %1",
        (self->source() != 0 ? self->source()->location().c_str() : "?") );
  self->childFailedId = 0;
  self->master()->childFailed(self->childDl, self, self->frontend);
  // Careful - self was probably deleted by above call
  return FALSE; // "Don't call me again"
}
//______________________________________________________________________

// New line of jigdo data arrived. This is similar to ConfigFile::rescan()
void JigdoIO::jigdoLine(string* l) {
  //debug("\"%1\"", l);
  string s;
  s.swap(*l);
  if (failed()) return;

  ++line;

  string::const_iterator x = s.begin(), end = s.end();
  // Empty line, or only contains '#' comment
  if (advanceWhitespace(x, end)) return;

  bool inComment = (section == "Comment" || section == "comment");
  if (*x != '[') {
    // This is a "Label=Value" line
    if (inComment) return;
    string labelName;
    while (!isWhitespace(*x) && *x != '=') { labelName += *x; ++x; }
    if (advanceWhitespace(x, end) || *x != '=')
      return generateError(_("No `=' after first word"));
    ++x; // Skip '='
    advanceWhitespace(x, end);
    vector<string> value;
    ConfigFile::split(value, s, x - s.begin());
    entry(&labelName, &value);
    return;
  }
  //____________________

  // This is a "[Section]" line
  ++x; // Advance beyond the '['
  if (advanceWhitespace(x, end)) // Skip space after '['
    return generateError(_("No closing `]' for section name"));
  string::const_iterator s1 = x; // s1 points to start of section name
  while (x != end && *x != ']' && !isWhitespace(*x) && *x != '['
         && *x != '=' && *x != '#') ++x;
  string::const_iterator s2 = x; // s2 points to end of section name
  if (advanceWhitespace(x, end))
    return generateError(_("No closing `]' for section name"));
  section.assign(s1, s2);
  //debug("Section `%1'", section);
  // In special case of "Include", format differs: URL after section name
  if (section == "Include") {
    string url;
    while (x != end && *x != ']') { url += *x; ++x; }
    int i = url.size();
    while (i > 0 && isWhitespace(url[--i])) { }
    url.erase(i + 1);
    include(&url);
  }
  if (*x != ']')
    return generateError(_("Section name invalid"));
  ++x; // Advance beyond the ']'
  if (!advanceWhitespace(x, end))
    return generateError(_("Invalid characters after closing `]'"));
}
//______________________________________________________________________

// "[Include url]" found - add
void JigdoIO::include(string* url) {
  string includeUrl;
  Download::uriJoin(&includeUrl, urlVal, *url);
  debug("Include `%1'", includeUrl);

  string leafname;
  auto_ptr<MakeImageDl::Child> childDl(
      master()->childFor(includeUrl, 0, &leafname));
  if (childDl.get() != 0) {
    string info = _("Retrieving .jigdo data");
    string destDesc = subst(Job::MakeImageDl::destDescTemplate(),
                            leafname, info);
    auto_ptr<DataSource::IO> frontend(
        master()->io->makeImageDl_new(childDl->source(), includeUrl,
                                      destDesc) );
    JigdoIO* jio = new JigdoIO(childDl.get(), includeUrl, frontend.get(),
                               this, line);
    childDl->setChildIo(jio);
    frontend.release();
    master()->io->job_message(&info);
    (childDl.release())->source()->run();
  }
}
//______________________________________________________________________

void JigdoIO::entry(string* label, vector<string>* value) {
# if DEBUG
  string s;
  for (vector<string>::iterator i = value->begin(), e = value->end();
       i != e; ++i) { s += ConfigFile::quote(*i); s += ' '; }
  debug("[%1] %2=%3", section, label, s);
# endif

  if (section == "Jigdo") {
    if (*label == "Version") {
      if (value->size() < 1) return generateError(_("Missing argument"));
      int ver = 0;
      string::const_iterator i = value->front().begin();
      string::const_iterator e = value->front().end();
      while (i != e && *i >= '0' && *i <= '9') {
        ver = 10 * ver + *i - '0';
        ++i;
      }
      if (ver > SUPPORTED_FORMAT)
        return generateError(_("Upgrade of jigdo required - this .jigdo file"
                               " requires a newer version of the program"));
    }
  }

}

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  IO object for downloads of .jigdo URLs; download, gunzip, interpret

*/

#include <config.h>

#include <memory>

#include <configfile.hh>
#include <debug.hh>
#include <jigdo-io.hh>
#include <log.hh>
#include <makeimagedl.hh>
#include <md5sum.hh>
#include <mimestream.hh>
#include <uri.hh>
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

// Root object
JigdoIO::JigdoIO(MakeImageDl::Child* c, const string& url,
                 DataSource::IO* frontendIo)
  : childDl(c), urlVal(url), frontend(frontendIo), parent(0), includeLine(0),
    firstChild(0), next(0), rootAndImageSectionCandidate(this), line(0),
    section(), imageSectionLine(0), imageName(), imageInfo(),
    imageShortInfo(), templateUrl(), templateMd5(0), childFailedId(0),
    gunzip(this) { }

// Non-root, i.e. [Include]d object
JigdoIO::JigdoIO(MakeImageDl::Child* c, const string& url,
                 DataSource::IO* frontendIo, JigdoIO* parentJigdo,
                 int inclLine)
  : childDl(c), urlVal(url), frontend(frontendIo), parent(parentJigdo),
    includeLine(inclLine), firstChild(0), next(0),
    rootAndImageSectionCandidate(parent->root()), line(0), section(),
    imageSectionLine(0), imageName(), imageInfo(), imageShortInfo(),
    templateUrl(), templateMd5(0), childFailedId(0), gunzip(this) {
  //debug("JigdoIO: Parent of %1 is %2", url, parent->urlVal);
}
//______________________________________________________________________

JigdoIO::~JigdoIO() {
  debug("~JigdoIO");

  if (childFailedId != 0) {
    g_source_remove(childFailedId);
    childFailedId = 0;
    master()->childFailed(childDl, this, frontend);
  }

  /* Don't delete children; master will do this! If we deleted them here,
     MakeImageDl::Child::childIoVal would be left dangling. */
//   // Delete all our children
//   JigdoIO* x = firstChild;
//   while (x != 0) {
//     JigdoIO* y = x->next;
//     delete x;
//     x = y;
//   }

  delete templateMd5;

  if (source() != 0) {
    source()->io.remove(this);
    Paranoid(source()->io.get() != this);
  }
}
//______________________________________________________________________

Job::IO* JigdoIO::job_removeIo(Job::IO* rmIo) {
  debug("job_removeIo %1", rmIo);
  if (rmIo == this) {
    // Do not "delete this" - this is called from ~JigdoIO above
    DataSource::IO* c = frontend;
    frontend = 0;
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

  if (gunzip.nextOut() > gunzipBuf) {
    debug("job_succeeded: No newline at end");
    ++line;
    const char* lineChars = reinterpret_cast<const char*>(gunzipBuf);
    if (g_utf8_validate(lineChars, gunzip.nextOut()-gunzipBuf, NULL) != TRUE)
      return generateError(_("Input .jigdo data is not valid UTF-8"));
    string line(lineChars, gunzip.nextOut() - gunzipBuf);
    jigdoLine(&line);
    if (failed()) return;
  }

  if (sectionEnd().failed()) return;
  setFinished();
  XStatus st = imgSect_eof();
  if (st.xfailed()) return;
  if (frontend != 0) frontend->job_succeeded();
  master()->childSucceeded(childDl, this, frontend);
  if (st.returned(1)) master()->jigdoFinished(); // Causes "delete this"
}

void JigdoIO::job_failed(string* message) {
  Paranoid(!failed());
  if (failed()) return;
  if (frontend != 0) frontend->job_failed(message);
  string err = _("Download of .jigdo file failed");
  master()->generateError(&err);
  /* We cannot call this right now:
     master()->childFailed(childDl, this, frontend);
     so schedule a callback to call it later. */
  childFailedId = g_idle_add_full(G_PRIORITY_HIGH_IDLE,&childFailed_callback,
                                  (gpointer)this, NULL);
  Paranoid(childFailedId != 0);
  imageName.assign("", 1); Paranoid(failed());
}

void JigdoIO::job_message(string* message) {
  if (failed()) return;
  if (frontend != 0) frontend->job_message(message);
}

void JigdoIO::dataSource_dataSize(uint64 n) {
  if (failed()) return;
  if (frontend != 0) frontend->dataSource_dataSize(n);
}

void JigdoIO::dataSource_data(const byte* data, unsigned size,
                              uint64 currentSize) {
  Assert(!finished());
  if (/*master()->finalState() ||*/ failed()) {
    debug("Got %1 bytes, ignoring", size);
    return;
  }
  //Assert(master()->state() == MakeImageDl::DOWNLOADING_JIGDO);
  debug("Got %1 bytes, processing", size);
  try {
    gunzip.inject(data, size);
  } catch (Error e) {
    ++line;
    generateError(e.message);
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
  if (failed()) return;

  // Look for end of line.
  byte* p = decompressed;
  const byte* end = decompressed + size;
  const byte* stringStart = gunzipBuf;
  string line;

  while (p < end) {
    if (*p == '\n') {
      // Process new line
      Paranoid(static_cast<unsigned>(p - stringStart) <= GUNZIP_BUF_SIZE);
      Paranoid(line.empty());
      const char* lineChars = reinterpret_cast<const char*>(stringStart);
      if (g_utf8_validate(lineChars, p - stringStart, NULL) != TRUE)
        throw Error(_("Input .jigdo data is not valid UTF-8"));
      line.append(lineChars, p - stringStart);
      jigdoLine(&line);
      if (failed()) return;
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
    if (failed()) return;
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

void JigdoIO::generateError(const string& msg) {
  string err;
  const char* fmt = (finished() ?
                     _("%1 (at end of %3)") : _("%1 (line %2 in %3)"));
  err = subst(fmt, msg, line,
              (source() != 0 ? source()->location().c_str() : "?") );
  generateError_plain(&err);
}

void JigdoIO::generateError(const char* msg) {
  string err;
  const char* fmt = (finished() ?
                     _("%1 (at end of %3)") : _("%1 (line %2 in %3)"));
  err = subst(fmt, msg, line,
              (source() != 0 ? source()->location().c_str() : "?") );
  generateError_plain(&err);
}

void JigdoIO::generateError_plain(string* err) {
  debug("generateError: %1", err);
  Paranoid(!failed());
  if (failed()) return;
  if (frontend != 0) frontend->job_failed(err);
  *err = _("Error processing .jigdo file contents");
  master()->generateError(err);

  /* We cannot call this right now:
     master()->childFailed(childDl, this, frontend);
     so schedule a callback to call it later. */
  childFailedId = g_idle_add_full(G_PRIORITY_HIGH_IDLE,&childFailed_callback,
                                  (gpointer)this, NULL);
  Paranoid(childFailedId != 0);
  imageName.assign("", 1); Paranoid(failed());
}

gboolean JigdoIO::childFailed_callback(gpointer data) {
  JigdoIO* self = static_cast<JigdoIO*>(data);
  debug("childFailed_callback for %1",
        (self->source() != 0 ? self->source()->location().c_str() : "?") );
  self->childFailedId = 0;
  self->master()->childFailed(self->childDl, self, self->frontend);
  self->master()->jigdoFinished(); // "delete self"
  return FALSE; // "Don't call me again"
}
//______________________________________________________________________

// Finding the first [Image] section

/* While scanning the tree of [Include]d .jigdo files, only the first [Image]
   section is relevant. IOW, we do a depth-first search of the tree. However,
   the .jigdo files are downloaded in parallel, and we want to pass on the
   image info as soon as possible. For this reason, we maintain an "image
   section candidate pointer", one for the whole include tree.

   If during the scanning of jigdo data we encounter an [Image] section AND
   imgSectCandidate()==this, then that section is the first such section in
   depth-first-order in the whole tree.

   If instead we encounter an [Include], the included file /might/ contain an
   image section, so we descend by setting imgSectCandidate() to the newly
   created child download. However, it can turn out the child does not
   actually contain an image section. In this case, we go back up to its
   parent.

   This is where it gets more complicated: Of course, the parent's data
   continued to be downloaded while we were wasting our time waiting for the
   last lines of the child, to be sure those last lines didn't contain an
   image section. After the point where we descended into the child, any
   number of [Include]s and /maybe/ an [Image] somewhere inbetween the
   [Include]s could have been downloaded. To find out whether this was the
   case, a quick depth-first scan of the tree is now necessary, up to the
   next point where we "hang" again because some .jigdo file has not been
   downloaded completely.

   The whole code is also used to find out when all JigdoIOs have finished -
   this could be done in simpler ways just by counting the active ones, but
   it comes "for free" with this code. */

// New child created due to [Include] in current .jigdo data
void JigdoIO::imgSect_newChild(JigdoIO* child) {
  if (master()->finalState() || imgSectCandidate() != this) return;
  debug("imgSect_newChild%1: From %2:%3 to child %4",
        (master()->haveImageSection() ? "(haveImageSection)" : ""),
        urlVal, line, child->urlVal);
  setImgSectCandidate(child);
}

// An [Image] section just ended - maybe it was the first one?
void JigdoIO::imgSect_parsed() {
  //debug("imgSect_parsed: %1 %2 %3", imgSectCandidate(), this, master()->finalState());
  if (master()->finalState() || imgSectCandidate() != this) return;
  debug("imgSect_parsed%1: %2:%3", (master()->haveImageSection()
        ? "(haveImageSection)" : ""), urlVal, line - 1);
  if (master()->haveImageSection()) return;
  master()->setImageSection(&imageName, &imageInfo, &imageShortInfo,
                            &templateUrl, &templateMd5);
}

#if DEBUG
namespace {
  inline const char* have(MakeImageDl* master) {
    if (master->haveImageSection())
      return "I";
    else
      return " ";
  }
}
#endif

// The end of the file was hit
XStatus JigdoIO::imgSect_eof() {
  MakeImageDl* m = master();
  if (m->finalState() || imgSectCandidate() != this) return OK;

  JigdoIO* x = parent; // Current position in tree
  int l = includeLine; // Line number in x, 0 if at start
  JigdoIO* child = this; // child included at line l of x, null if l==0

  while (x != 0) {
#   if DEBUG
    const char* indentStr = "                                        ";
    const char* indent = indentStr + 40;
    JigdoIO* ii = x;
    while (ii != 0) { indent -= 2; ii = ii->parent; }
    if (indent < indentStr) indent = indentStr;
    debug("imgSect_eof:%1%2Now at %3:%4", have(m), indent, x->urlVal, l);
#   endif
    JigdoIO* nextChild;
    if (l == 0) nextChild = x->firstChild; else nextChild = child->next;

    if (nextChild != 0) {
      /* Before moving l to the line of the next [Include], check whether the
         area of the file that l moves over contains an [Image] */
      if (l < x->imageSectionLine
          && x->imageSectionLine < nextChild->includeLine) {
        debug("imgSect_eof:%1%2Found before [Include]", have(m), indent);
        if (!m->haveImageSection())
          m->setImageSection(&x->imageName, &x->imageInfo,
              &x->imageShortInfo, &x->templateUrl, &x->templateMd5);
      }
      // No [Image] inbetween - move on, descend into [Include]
      debug("imgSect_eof:%1%2Now at %3:%4, descending",
            have(m), indent, x->urlVal, nextChild->includeLine);
      x = nextChild;
      l = 0;
      child = 0;
      continue;
    }

    // x has no more children - but maybe an [Image] at the end?
    if (l < x->imageSectionLine) {
      debug("imgSect_eof:%1%2Found after last [Include], if any",
            have(m), indent);
      if (!m->haveImageSection())
        m->setImageSection(&x->imageName, &x->imageInfo,
                       &x->imageShortInfo, &x->templateUrl, &x->templateMd5);
    }

    // Nothing found. If x not yet fully downloaded, stop here
    if (!x->finished()) {
      debug("imgSect_eof:%1%2Waiting for %3 to download",
            have(m), indent, x->urlVal);
      setImgSectCandidate(x);
      return OK;
    }

    // Nothing found and finished - go back up in tree
    debug("imgSect_eof:%1%2Now at end of %3, ascending",
          have(m), indent, x->urlVal);
    l = x->includeLine;
    child = x;
    x = x->parent;
  }
  if (m->haveImageSection()) {
    debug("imgSect_eof: Finished");
    return XStatus(1);
  } else {
    generateError(_("No `[Image]' section found in .jigdo data"));
    return FAILED;
  }
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
//     vector<string> value;
//     ConfigFile::split(value, s, x - s.begin());
//     entry(&labelName, &value);
    entry(&labelName, &s, x - s.begin());
    return;
  }
  //____________________

  // This is a "[Section]" line
  if (sectionEnd().failed()) return;
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

  // In special case of "Image", ignore 2nd and subsequent sections
  if (section == "Image") {
    if (imageSectionLine == 0)
      imageSectionLine = line;
    else
      section += "(ignored)";
  }
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

Status JigdoIO::sectionEnd() {
  if (section != "Image") return OK;
  // Section that just ended was [Image]
  const char* valueName = 0;
  if (templateMd5 == 0) valueName = "Template-MD5Sum";
  if (templateUrl.empty()) valueName = "Template";
  if (imageName.empty()) valueName = "Filename";
  if (valueName == 0) {
    imgSect_parsed();
    return OK;
  }
  // Error: Not all required fields found
  --line;
  string s = subst(_("`%1=...' line missing in [Image] section"), valueName);
  generateError(s);
  return FAILED;
}
//______________________________________________________________________

// "[Include url]" found - add
void JigdoIO::include(string* url) {
  string includeUrl;
  uriJoin(&includeUrl, urlVal, *url);
  debug("%1:[Include %2]", line, includeUrl);

  JigdoIO* p = this;
  do {
    //debug("include: Parent of %1 is %2", p->urlVal,
    //      (p->parent ? p->parent->urlVal : "none"));
    if (p->urlVal == includeUrl)
      return generateError(_("Loop of [Include] directives"));
    p = p->parent;
  } while (p != 0);

  string leafname;
  auto_ptr<MakeImageDl::Child> childDl(
      master()->childFor(includeUrl, 0, &leafname));
  if (childDl.get() != 0) {
    MakeImageDl::IO* mio = master()->io.get();
    string info = _("Retrieving .jigdo data");
    string destDesc = subst(Job::MakeImageDl::destDescTemplate(),
                            leafname, info);
    auto_ptr<DataSource::IO> frontend(0);
    if (mio != 0)
      frontend.reset(mio->makeImageDl_new(childDl->source(), includeUrl,
                                          destDesc) );
    JigdoIO* jio = new JigdoIO(childDl.get(), includeUrl, frontend.get(),
                               this, line);
    childDl->setChildIo(jio);
    frontend.release();
    if (mio != 0) mio->job_message(&info);

    // Add new child
    JigdoIO** jiop = &firstChild;
    while (*jiop != 0) jiop = &(*jiop)->next;
    *jiop = jio;

    imgSect_newChild(jio);

    (childDl.release())->source()->run();
  }
}
//______________________________________________________________________

namespace {
  // For Base64In - put decoded bytes into 16-byte array
  struct ArrayOut {
    typedef ArrayOut& ResultType;
    ArrayOut() { }
    void set(byte* array) { cur = array; end = array + 16; }
    void put(byte b) { if (cur == end) cur = end = 0; else *cur++ = b; }
    ArrayOut& result() { return *this; }
    byte* cur; byte* end;
  };
}
//____________________

/* @param label Pointer to word before the '='
   @param data Pointer to string containing whole input line
   @param valueOff Offset of value (part after '=') in data */
void JigdoIO::entry(string* label, string* data, unsigned valueOff) {
  vector<string> value;
  ConfigFile::split(value, *data, valueOff);
# if DEBUG
  string s;
  for (vector<string>::iterator i = value.begin(), e = value.end();
       i != e; ++i) { s += '>'; s += *i; s += "< "; }
  // { s += ConfigFile::quote(*i); s += ' '; }
  debug("%1:[%2] %3=%4", line, section, label, s);
# endif
  //____________________

  if (section == "Include") {

    return generateError(_("A new section must be started after [Include]"));
    //____________________

  } else if (section == "Jigdo") {
    if (*label == "Version") {
      if (value.empty()) return generateError(_("Missing argument"));
      int ver = 0;
      string::const_iterator i = value.front().begin();
      string::const_iterator e = value.front().end();
      while (i != e && *i >= '0' && *i <= '9') {
        ver = 10 * ver + *i - '0';
        ++i;
      }
      if (ver > SUPPORTED_FORMAT)
        return generateError(_("Upgrade required - this .jigdo file needs "
                               "a newer version of the jigdo program"));
    }
    //____________________

  } else if (section == "Image") {

    /* Only called for first [Image] section in file - for further sections,
       section=="Image(ignored)". Does some sanity checks on the supplied
       data. */
    if (*label == "Filename") {
      if (!imageName.empty()) return generateError(_("Value redefined"));
      if (value.empty()) return generateError(_("Missing argument"));
      // Only use leaf name, ignore dirname delimiters, max 100 chars
      string::size_type lastSlash = value.front().rfind('/');
      string::size_type lastSep = value.front().rfind(DIRSEP);
      if (lastSlash > lastSep) lastSep = lastSlash;
      imageName.assign(value.front(), lastSep + 1, 100);
      if (imageName.empty()) return generateError(_("Invalid image name"));
    } else if (*label == "Template") {
      if (!templateUrl.empty()) return generateError(_("Value redefined"));
      if (value.empty()) return generateError(_("Missing argument"));
      // Immediately turn template URL into absolute URL if necessary
      uriJoin(&templateUrl, urlVal, value.front());
    } else if (*label == "Template-MD5Sum") {
      if (templateMd5 != 0) return generateError(_("Value redefined"));
      if (value.empty()) return generateError(_("Missing argument"));
      templateMd5 = new MD5();
      // Helper class places decoded bytes into MD5 object
      Base64In<ArrayOut> decoder;
      decoder.result().set(templateMd5->sum);
      decoder << value.front();
      if (decoder.result().cur == 0
          || decoder.result().cur != decoder.result().end) {
        delete templateMd5; templateMd5 = 0;
        return generateError(_("Invalid Template-MD5Sum argument"));
      }
      // For security, double-check the value
      Base64String b64;
      b64.write(templateMd5->sum, 16).flush();
      if (b64.result() != value.front()) {
        debug("b64='%1' value='%2'", b64.result(), value.front());
        return generateError(_("Invalid Template-MD5Sum argument"));
      }
    } else if (*label == "ShortInfo") {
      // ShortInfo is 200 chars max
      if(!imageShortInfo.empty()) return generateError(_("Value redefined"));
      imageShortInfo.assign(*data, valueOff, 200);
    } else if (*label == "Info") {
      // ImageInfo is 5000 chars max
      if (!imageInfo.empty()) return generateError(_("Value redefined"));
      imageInfo.assign(*data, valueOff, 5000);
    }
    //____________________

  } else if (section == "Parts") {

    if (value.empty()) return generateError(_("Missing argument"));
    MD5 md5;
    Base64In<ArrayOut> decoder;
    decoder.result().set(md5.sum);
      decoder << *label;
      if (decoder.result().cur == 0
          || decoder.result().cur != decoder.result().end) {
        return generateError(_("Invalid MD5Sum in Parts section"));
      }
      // For security, double-check the value
      Base64String b64;
      b64.write(md5.sum, 16).flush();
      if (b64.result() != *label) {
        debug("x b64='%1' value='%2'", b64.result(), *label);
        return generateError(_("Invalid MD5Sum in Parts section"));
      }
      //debug("PART %1 -> %2", md5.toString(), value.front());
      master()->addPart(urlVal, md5, value);

  } else if (section == "Servers") {

    if (value.empty()) return generateError(_("Missing argument"));
    if (master()->addServer(urlVal, *label, value).failed())
      return generateError(_("Recursive label definition"));

  } // endif (section == "Something")

}

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  #test-deps job/datasource.o util/gunzip.o util/configfile.o util/md5sum.o
  #test-deps util/glibc-md5.o net/uri.o job/url-mapping.o
  #test-ldflags $(LIBS)

*/

#define DEBUG 1
#include <config.h>

#include <limits.h>
#include <string.h>

#include <makeimagedl.hh>
#include <md5sum.hh>
#include <url-mapping.hh>

#include <jigdo-io.hh>
/* Cannot link against jigdo-io.o, because this is not always compiled with
   DEBUG==1 */
#include <jigdo-io.cc>
//______________________________________________________________________

using namespace Job;
typedef MakeImageDl::Child Child;

namespace {

  GMainLoop* gLoop = 0;

  // Map of URL to content at that URL
  typedef map<string, const char*> MapWww;
  MapWww www;

  /* Special DataSource which outputs data from memory */
  class MemData : public Job::DataSource {
  public:
    /* If contents == 0, will call job_failed() */
    MemData(DataSource::IO* ioPtr, const string& loc, const char* contents)
      : DataSource(), locVal(loc),
        data(reinterpret_cast<const byte*>(contents)),
        cur(data), dataEnd(data) {
      if (ioPtr) io.addListener(*ioPtr);
      if (contents != 0) dataEnd += strlen(contents);
      msg("MemData %1 len=%2", location(), dataEnd - data);
    }
    virtual ~MemData();

    virtual void run() {
      if (data == 0) {
        msg("MemData fail %1", location());
        string err = "Failed";
        //io->job_failed(&err);
        IOSOURCE_SEND(DataSource::IO, io, job_failed, (err));
      }
    }

    virtual bool paused() const { return false; }
    virtual void pause() { }
    virtual void cont() { }

    virtual const Progress* progress() const { return 0; }
    virtual const string& location() const { return locVal; }

    void reset() { cur = data; }

    bool finished() { return cur == dataEnd; }

    /* Give n lines of data to ioPtr. default means all data. Must be called
       manually to feed data to the rest of the code, since we do not use the
       glib idle loop. */
    void output(unsigned n = UINT_MAX) {
      const byte* newCur = cur;
      while (newCur < dataEnd && n > 0) {
        if (*newCur == '\n') --n;
        ++newCur;
      }
      if (newCur == cur) return;
      IOSOURCE_SEND(DataSource::IO, io,
                    dataSource_data, (cur, newCur - cur, newCur - data));
      cur = newCur;
      if (cur == dataEnd)
        IOSOURCE_SEND(DataSource::IO, io, job_succeeded, ());
    }

  private:
    string locVal;
    const byte* data;
    const byte* cur;
    const byte* dataEnd;
  };

  MemData::~MemData() {
    msg("~MemData %1", location());
  }
  //______________________________________________________________________

  // Get MemData from Child
  inline MemData* memData(Child* c) {
    MemData* result = dynamic_cast<MemData*>(c->source());
    Assert(result != 0);
    return result;
  }

  // Get MemData from MakeImageDl and URL
  MemData* memData(MakeImageDl& m, const char* url) {
    Assert(!m.children().empty());
    MemData* result = 0;
    typedef MakeImageDl::ChildList::const_iterator Iter;
    for (Iter i = m.children().begin(), e = m.children().end(); i != e; ++i){
      if (i->get()->source()->location() == url) {
        result = dynamic_cast<MemData*>(i->get()->source());
        break;
      }
    }
    if (result == 0)
      msg("No download of %1 currently active", url);
    Assert(result != 0);
    return result;
  }

  inline void output(MakeImageDl& m, const char* url, size_t n = UINT_MAX) {
    memData(m, url)->output(n);
  }
  //______________________________________________________________________

  // Execute callbacks
  void idle() {
    while (g_main_context_iteration(0, FALSE) == true) { }
  }
  //______________________________________________________________________

  const char* const hexDigits = "0123456789abcdef";
  void escapedChar(string* o, byte c) {
    switch (c) {
    case 0: *o += "\\0"; break;
    case '\n': *o += "\\n"; break;
    case '\t': *o += "\\t"; break;
    case '"': case '\\': *o += '\\'; *o += c; break;
    default:
      if (c >= ' ' && c <= '~') {
        *o += c;
      } else {
        *o += "\\x";
        *o += hexDigits[unsigned(c) >> 4];
        *o += hexDigits[unsigned(c) & 0xfU];
      }
    }
  }

  inline string escapedString(const string& s) {
    string result;
    for (unsigned i = 0; i < s.length(); ++i)
      escapedChar(&result, s[i]);
    return result;
  }
  //______________________________________________________________________

  /* We want to record the "...: imgSect..." lines. */
  bool jigdoIoEnabled = false;
  string imgSectLogged;
  void loggerPut(const string& unitName, unsigned char unitNameLen,
                 const char* format, int args, const Subst arg[]) {
    if (unitName == "jigdo-io") {
      if (strncmp(format, "imgSect", 7) == 0
          || strncmp(format, "generateError", 13) == 0) {
        imgSectLogged += Subst::subst(format, args, arg);
        imgSectLogged += '\n';
      }
      if (!jigdoIoEnabled) return;
    }
    Logger::defaultPut(unitName, unitNameLen, format, args, arg);
  }
  void loggerInit() {
    Logger::setOutputFunction(&loggerPut);
    Logger* l = Logger::enumerate();
    while (l != 0 && strcmp(l->name(), "jigdo-io") == 0)
      l = Logger::enumerate(l);
    Assert(l != 0); // jigdo-io.cc must have been compiled with DEBUG==1
    jigdoIoEnabled = l->enabled();
    if (!jigdoIoEnabled) Logger::setEnabled("jigdo-io");
  }

} // namespace

//======================================================================

MakeImageDl::Child* MakeImageDl::childFor(const string& url, const MD5* md,
                                          string* leafnameOut, Child*) {
  Assert(md == 0);
  if (leafnameOut != 0) *leafnameOut = url;

  // Look up content
  const char* contents;
  MapWww::iterator i = www.find(url);
  if (i == www.end()) contents = 0; else contents = i->second;

  auto_ptr<MemData> dl(new MemData(0, url, contents));
  Child* c = new Child(this, &childrenVal, dl.get(), 0);
  dl.release();
  c->childSuccFail = true;
  return c;
}

MakeImageDl::MakeImageDl(/*IO* ioPtr,*/ const string& jigdoUri,
                         const string& destination)
    : io(/*ioPtr*/), stateVal(DOWNLOADING_JIGDO),
      jigdoUrl(jigdoUri), childrenVal(), dest(destination),
      tmpDirVal("/tmp"), mi(),
      imageNameVal(), imageInfoVal(), imageShortInfoVal(), templateUrls(0),
      templateMd5Val(0) {
  if (!jigdoUri.empty()) {
    Child* a = childFor(jigdoUri);
    JigdoIO* jio = new JigdoIO(a, jigdoUrl);
    a->source()->io.addListener(*jio);
    a->source()->run();
  }
}

Job::MakeImageDl::~MakeImageDl() { }

// const char* Job::MakeImageDl::destDescTemplateVal =
//     _("Cache entry %1  --  %2");

void MakeImageDl::childFailed(Child* childDl) {
  msg("childFailed: %1",
      childDl->source() ? childDl->source()->location() :"[deleted source]");
  // No: delete childDl;
}

// void MakeImageDl::childSucceeded(Child* childDl, DataSource::IO* /*chldIo*/) {
//   msg("childSucceeded: %1",
//       childDl->source() ? childDl->source()->location() :"[deleted source]");
//   // No: delete childDl;
// }

void MakeImageDl::setImageSection(string* imageName, string*, string*,
                                  PartUrlMapping*, MD5**) {
  Paranoid(!haveImageSection());
  imageNameVal.swap(*imageName);
}

void MakeImageDl::jigdoFinished() {
  debug("jigdoFinished");
}

void Job::MakeImageDl::killAllChildren() {
  debug("killAllChildren");
}

void MakeImageDl::Child::job_deleted() { }
void MakeImageDl::Child::job_succeeded() { }
void MakeImageDl::Child::job_failed(const string&) { }
void MakeImageDl::Child::job_message(const string&) { }
void MakeImageDl::Child::dataSource_dataSize(uint64) { }
void MakeImageDl::Child::dataSource_data(const byte*, unsigned, uint64) { }
//======================================================================

// Basic check: Does it find the image section?
void testSimple() {
  msg("---------------------------------------- testSimple");
  www.insert(make_pair("http://simple",
    "[Jigdo]\n"
    "Version =   0  #\n"
    "Version=1.1\n"
    "Generator=jigdo-file/0.7.0\n"
    "\n"
    "[Servers]\n"
    "X=http://10.0.0.5/~richard/ironmaiden/\n"
    "X=http://localhost/~richard/ironmaiden/fb-\n"
    "\n"
    "[Image]\n"
    "Filename=image\n"
    "Template=image.template\n"
    "Template-MD5Sum=h5FAyHqEsvXSTuGUNdhzJw\n"
    "xx       =       8\\ 9 a b c' \"'\"d' e\" #ffo\n"
    "ShortInfo='\"Debian GNU/Linux 3.0 r1 \\\"Woody\\\" - i386 B-$num\"'\n"
    "Info='Generated on Sun, 16 Mar 2003 04:45:40 -0700'\n"
    "\n"
    "[Image]\n"
    "Filename=image\n"
    "Template=image.template\n"
    "Template-MD5Sum=h5FAyHqEsvXSTuGUNdhzJw\n"
    "\n"
    "[Parts]\n"
    "FsGFXNwcbCdvWTamkRdp7g=X:part4\n"
    "H2-Rw7tuyjWdxVeqnS_vcw=X:part8\n"
    "gG64beTeh9nZWMVvv80zgw=X:part9\n"));
  MakeImageDl m("", "");
  imgSectLogged.clear();
  Child* a = m.childFor("http://simple");
  a->source()->io.addListener(*new JigdoIO(a, "http://simple"));
  while (a->source() != 0 && !memData(a)->finished()) { // Feed single bytes
    memData(a)->output(1);
    idle();
  }
  msg("logged: \"%1\"", escapedString(imgSectLogged));
  Assert(imgSectLogged == "imgSect_parsed: http://simple:17\n"
                          "imgSect_eof: Finished\n");
}

// Error message: No image section found
void testNoMD5() {
  msg("---------------------------------------- testNoMD5");
  www.insert(make_pair("http://no-md5",
    "[Image]\n"
    "Filename=image\n"
    "Template=image.template\n"));
  MakeImageDl m("", "");
  imgSectLogged.clear();
  Child* a = m.childFor("http://no-md5");
  a->source()->io.addListener(*new JigdoIO(a, "http://no-md5"));
  memData(a)->output(); idle();
  msg("logged: \"%1\"", escapedString(imgSectLogged));
  Assert(imgSectLogged == "generateError: `Template-MD5Sum=...' line missing"
         " in [Image] section (line 2 in http://no-md5)\n");
}

// Handle missing newline at end of file
void testMinimal() {
  msg("---------------------------------------- testMinimal");
  www.insert(make_pair("http://minimal",
    "[Image]\n"
    "Filename=image\n"
    "Template=image.template\n"
    "Template-MD5Sum=h5FAyHqEsvXSTuGUNdhzJw"));
  MakeImageDl m("", "");
  imgSectLogged.clear();
  Child* a = m.childFor("http://minimal");
  a->source()->io.addListener(*new JigdoIO(a, "http://minimal"));
  memData(a)->output();
  idle();
  msg("logged: \"%1\"", escapedString(imgSectLogged));
  Assert(imgSectLogged == "imgSect_parsed: http://minimal:4\n"
                          "imgSect_eof: Finished\n");
}

// Error: recursive include
void testLoop() {
  msg("---------------------------------------- testLoop");
  www.insert(make_pair("http://loop",
    "\t[ Include   http://simple   ]\n"
    "[Include http://loop]\n"));
  MakeImageDl m("", "");
  imgSectLogged.clear();
  Child* a = m.childFor("http://loop");
  a->source()->io.addListener(*new JigdoIO(a, "http://loop"));
  memData(a)->output(1); // Feed include simple line
  idle();
  output(m, "http://simple");
  idle();
  memData(a)->output(); // Feed loop include line
  idle();
  msg("logged: \"%1\"", escapedString(imgSectLogged));
  Assert(imgSectLogged ==
    "imgSect_newChild: From http://loop:1 to child http://simple\n"
    "imgSect_parsed: http://simple:17\n"
    "imgSect_eof:I  Now at http://loop:1\n"
    "imgSect_eof:I  Waiting for http://loop to download\n"
    "generateError: Loop of [Include] directives (line 2 in http://loop)\n");
}

// Deeper include tree
void testFork() {
  www.insert(make_pair("http://fork",
    "[Include http://fork1]\n"
    "[Include http://fork2]\n"
    "\n"
    "[Image]\n"
    "Filename=image\n"
    "Template=image.template\n"
    "Template-MD5Sum=h5FAyHqEsvXSTuGUNdhzJw"));
  www.insert(make_pair("http://fork1",
    "# yada\n"
    "[Servers]\n"
    "Y=http://hoohoo/\n"));
  www.insert(make_pair("http://fork2",
    "[Servers]\n"
    "X=http://booboo/\n"
    "\n"
    "[Include http://fork21]\n"
    "[Include http://fork22]\n"
    "\n"
    "[Comment]\n"));
  www.insert(make_pair("http://fork21",
    "# Empty\n"));
  www.insert(make_pair("http://fork22",
    "[Jigdo]\n"
    "Version=1.1\n"
    "\n"));

  {
    msg("---------------------------------------- testFork a");
    imgSectLogged.clear();
    MakeImageDl m("http://fork", "");
    output(m, "http://fork");
    output(m, "http://fork2");
    output(m, "http://fork21");
    output(m, "http://fork22");
    output(m, "http://fork1");
    msg("logged: \"%1\"", escapedString(imgSectLogged));
    Assert(imgSectLogged ==
      "imgSect_newChild: From http://fork:1 to child http://fork1\n"
      "imgSect_eof:   Now at http://fork:1\n"
      "imgSect_eof:   Now at http://fork:2, descending\n"
      "imgSect_eof:     Now at http://fork2:0\n"
      "imgSect_eof:     Now at http://fork2:4, descending\n"
      "imgSect_eof:       Now at http://fork21:0\n"
      "imgSect_eof:       Now at end of http://fork21, ascending\n"
      "imgSect_eof:     Now at http://fork2:4\n"
      "imgSect_eof:     Now at http://fork2:5, descending\n"
      "imgSect_eof:       Now at http://fork22:0\n"
      "imgSect_eof:       Now at end of http://fork22, ascending\n"
      "imgSect_eof:     Now at http://fork2:5\n"
      "imgSect_eof:     Now at end of http://fork2, ascending\n"
      "imgSect_eof:   Now at http://fork:2\n"
      "imgSect_eof:   Found after last [Include], if any\n"
      "imgSect_eof:I  Now at end of http://fork, ascending\n"
      "imgSect_eof: Finished\n");
  }

  {
    msg("---------------------------------------- testFork b");
    imgSectLogged.clear();
    MakeImageDl m("http://fork", "");
    output(m, "http://fork", 1);
    output(m, "http://fork1");
    output(m, "http://fork", 1);
    output(m, "http://fork2");
    output(m, "http://fork21");
    output(m, "http://fork22");
    output(m, "http://fork");
    msg("logged: \"%1\"", escapedString(imgSectLogged));
    Assert(imgSectLogged ==
      "imgSect_newChild: From http://fork:1 to child http://fork1\n"
      "imgSect_eof:   Now at http://fork:1\n"
      "imgSect_eof:   Waiting for http://fork to download\n"
      "imgSect_newChild: From http://fork:2 to child http://fork2\n"
      "imgSect_newChild: From http://fork2:4 to child http://fork21\n"
      "imgSect_eof:     Now at http://fork2:4\n"
      "imgSect_eof:     Now at http://fork2:5, descending\n"
      "imgSect_eof:       Now at http://fork22:0\n"
      "imgSect_eof:       Waiting for http://fork22 to download\n"
      "imgSect_eof:     Now at http://fork2:5\n"
      "imgSect_eof:     Now at end of http://fork2, ascending\n"
      "imgSect_eof:   Now at http://fork:2\n"
      "imgSect_eof:   Waiting for http://fork to download\n"
      "imgSect_parsed: http://fork:7\n"
      "imgSect_eof: Finished\n");
  }
}

// Image between includes
void testBetween() {
  www.insert(make_pair("http://between",
    "[Include http://fork2]\n"
    "[Image]\n"
    "Filename=image\n"
    "Template=image.template\n"
    "Template-MD5Sum=h5FAyHqEsvXSTuGUNdhzJw\n"
    "[Include http://fork1]\n"));

  msg("---------------------------------------- testBetween");
  imgSectLogged.clear();
  MakeImageDl m("http://between", "");
  output(m, "http://between");
  output(m, "http://fork1");
  output(m, "http://fork2");
  output(m, "http://fork22");
  output(m, "http://fork21");
  msg("logged: \"%1\"", escapedString(imgSectLogged));
  Assert(imgSectLogged ==
    "imgSect_newChild: From http://between:1 to child http://fork2\n"
    "imgSect_newChild: From http://fork2:4 to child http://fork21\n"
    "imgSect_eof:     Now at http://fork2:4\n"
    "imgSect_eof:     Now at http://fork2:5, descending\n"
    "imgSect_eof:       Now at http://fork22:0\n"
    "imgSect_eof:       Now at end of http://fork22, ascending\n"
    "imgSect_eof:     Now at http://fork2:5\n"
    "imgSect_eof:     Now at end of http://fork2, ascending\n"
    "imgSect_eof:   Now at http://between:1\n"
    "imgSect_eof:   Found before [Include]\n"
    "imgSect_eof:I  Now at http://between:6, descending\n"
    "imgSect_eof:I    Now at http://fork1:0\n"
    "imgSect_eof:I    Now at end of http://fork1, ascending\n"
    "imgSect_eof:I  Now at http://between:6\n"
    "imgSect_eof:I  Now at end of http://between, ascending\n"
    "imgSect_eof: Finished\n");
}
//______________________________________________________________________

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);
  loggerInit();
  gLoop = g_main_loop_new(0, FALSE);

  testSimple();
  testNoMD5();
  testMinimal();
  testLoop();
  testFork();
  testBetween();

  msg("Exit");
  return 0;
}

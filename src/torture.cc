/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  A "crashme" for jigdo: Throws test data at it.

  ./torture <number>
  Will always create the same test case for the same <number>. *Only*
  creates the files, doesn't work on them.

  ./torture <lownumber> <highnumber>
  Create a range of test cases *and run a MkTemplate op on them*,
  checking the result for correctness. Next, re-creates the image,
  then checks the created image's MD5Sum. Causes no end of disc
  thrashing - using a RAM disc is *highly* recommended for longer
  runs.

  Creates many files named `ironmaiden/part<somenumber>' and one file
  `ironmaiden/image', runs a MkTemplate::run() over them and lets it write
  `ironmaiden/image.template'. During the run(), the information on which
  files matched must be the same as calculated. After the run(), a
  verify() is done to check whether the template is really OK.

  Sorry, this is a quick hack.

  In very rare cases, this will incorrectly report a failure because
  it doesn't take into account that concatenated parts of an
  all-zeroes file may be large enough for the whole file to fit.

*/

#include <config.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <bstream.hh>
#include <jigdoconfig.hh>
#include <scan.hh>
#include <string.hh>
#include <md5sum.hh>
#include <mimestream.hh>
#include <mkimage.hh>
#include <mktemplate.hh>
#include <recursedir.hh>

// mingw doesn't define SSIZE_MAX (maximum amount to read() at a time)
#ifndef SSIZE_MAX
#  include <limits.h>
#  define SSIZE_MAX (UINT_MAX)
#endif

DEBUG_UNIT_LOCAL("torture")
//______________________________________________________________________

/* I sometimes run torture on machines on which 'niced', the nice
   daemon, is running. It can be used to stop/kill processes which
   need unusually much memory/CPU time. If you define CHEAT_MR_NICE=1,
   torture will change its pid every so often (by forking, then
   exiting in the parent), which prevents its being stopped/killed by
   niced.
   Use at own risk, and please ensure you're not violating usage terms
   by cheating 'niced'. */
#ifndef CHEAT_MR_NICE
#  define CHEAT_MR_NICE 0
#endif

#define TORTURE_DIR "ironmaiden" DIRSEPS

string cacheFile;
//______________________________________________________________________

#if HAVE_MMAP
#  include <sys/mman.h>
#else
// Dummy implementation of mmap which actually loads the data into memory
void* mmap(void *start, size_t length, int /*prot*/, int /*flags*/, int fd,
           off_t offset) {
  Assert(start == 0 && offset == 0);
  byte* buf = new byte[length];
  byte* bufPtr = buf;
  size_t toRead = length;
  while (toRead > 0) {
    size_t didRead = read(fd, bufPtr,
                          (toRead < SSIZE_MAX ? toRead : SSIZE_MAX));
    if (didRead == 0) break;
    toRead -= didRead;
    bufPtr += didRead;
  }
  return buf;
}
// Dummy munmap frees the memory
int munmap(void *start, size_t /*length*/) {
  delete[] static_cast<byte*>(start);
  return 0;
}
#define PROT_READ 0
#define MAP_SHARED 0
#endif
//______________________________________________________________________

namespace {

# if CHEAT_MR_NICE

# include <signal.h>

  pid_t parentPid;

  /* Calling this may have the side-effect of closing all file
     streams! */
  void cheatMrNice() {
    pid_t pid = fork();
    if (pid == -1 || pid == 0) return; // if error or child
    //cerr << "     [changed pid to " << pid << ']' << endl;
    if (getpid() == parentPid)
      pause(); // Wait until killed by last child
    exit(0);
  }

# else /*________________________________________*/

  inline void cheatMrNice() { }

# endif
  //______________________________________________________________________

  // approximate maximum accumulated size of files
  const size_t MAX_MEM = 8*1024*1024;
  // approximate size of created image
  const size_t MAX_IMAGE = 16*1024*1024;

  void update(MD5Sum& md, uint32 x) {
    md.update(static_cast<byte>(x));
    md.update(static_cast<byte>(x >> 8));
    md.update(static_cast<byte>(x >> 16));
    md.update(static_cast<byte>(x >> 24));
  }

  struct Rand {
    MD5Sum md;
    struct {
      uint32 nr;
      uint32 serial;
      MD5 r;
    } hashData;
    byte* rptr; // points to one of hashData.r's elements
    byte* rend;
    uint32 res; // Bit reservoir
    size_t bitsInRes;
    bool msg;

    Rand(uint32 nr, bool printMessages = false) {
      hashData.nr = nr;
      hashData.serial = 0;
      hashData.r.clear();
      rptr = rend = &hashData.r.sum[0] + 16;
      res = 0;
      bitsInRes = 0;
      msg = printMessages;
    }
    // Create another 128 semi-random bits in md
    void thumbScrew();
    // Return n semi-random bits, n <= 24
    uint32 get(size_t n) {
      while (bitsInRes < n) {
        if (rptr == rend) thumbScrew();
        res |= (*rptr++) << bitsInRes;
        bitsInRes += 8;
      }
      uint32 r = res & ((1 << n) - 1);
      res >>= n;
      bitsInRes -= n;
      return r;
    }
    // Return an integer in the range 0...n-1
    uint32 rnd(size_t n) {
      return static_cast<uint32>(
               static_cast<uint64>(get(24)) * n / 0x1000000);
    }
  };
  void Rand::thumbScrew() {
    md.reset();
    update(md, hashData.nr);
    update(md, hashData.serial);
    md.update(&hashData.r.sum[0], 16 * sizeof(byte));
    md.finishForReuse();
    hashData.r = md;
    ++hashData.serial;
    rptr = &hashData.r.sum[0];
    //cout << '<' << hashData.r << '>' << endl;
  }
  //______________________________________________________________________

  class File {
  public:
    explicit File(const char* fileName, size_t s = 0, size_t n = 0);
    File() : data(0) { }
    inline File(const File& f);
    inline File& File::operator=(const File& f);
    inline ~File();
    size_t size;
    size_t nr;
    byte* data;
  private:
    int* refCount;
  };
  struct Match {
    Match(uint64 o = 0, size_t n = 0) : off(o), nr(n) { }
    uint64 off; // Offset in image of match
    size_t nr; // Nr of file that matched
  };
  vector<File> files;
  uint32 lastFilesNr = 1; // Nr of test case
  vector<Match> imageMatches; // matches written with last call to mkimage()
  //____________________

  File::File(const char* fileName, size_t s, size_t n) : size(s), nr(n) {
    refCount = new int;
    *refCount = 1;
    int fd = open(fileName, O_RDONLY);
    if (fd == -1) {
      cerr << "Couldn't open " << fileName << " (" << strerror(errno) << ')'
          << endl;
      abort();
    }
    // mmap the file
    data = reinterpret_cast<byte*>(mmap(0, s, PROT_READ, MAP_SHARED, fd, 0));
    if (data == ((byte*)-1)) {
      cerr << "Couldn't mmap (" << strerror(errno) << ')' << endl;
      abort();
    }
    //cerr << "mmapped " << fileName << " at " << (void*)data << "..."
    //     << (void*)(data+s) << endl;
    close(fd);
  }

  File& File::operator=(const File& f) {
    if (this == &f) return *this;
    if (data != 0 && --*refCount == 0) {
      munmap(reinterpret_cast<char*>(data), size);
      //cerr << "munmapped " << (void*)data << endl;
      delete refCount;
    }
    size = f.size;
    nr = f.nr;
    data = f.data;
    refCount = f.refCount;
    if (data != 0) ++*refCount;
    return *this;
  }

  File::File(const File& f) {
    size = f.size;
    nr = f.nr;
    data = f.data;
    refCount = f.refCount;
    if (data != 0) ++*refCount;
  }

  File::~File() {
    if (data != 0 && --*refCount == 0) {
      munmap(reinterpret_cast<char*>(data), size);
      //cerr << "munmapped " << (void*)data << endl;
      delete refCount;
    }
  }
  //____________________

  void mkfiles(uint32 nr) {
    nr &= ~127; // Files only change every 128 test cases
    if (lastFilesNr == nr) return;
    lastFilesNr = nr;
    Rand rand(nr);
    size_t mem = 0;
    files.resize(0);
    //int zeroFile = -1; // Only one file of zeroes per test case
    // Disable all-zeroes file - causes too many false FAIL messages
    int zeroFile = 0;

    ifstream rev(TORTURE_DIR "rev");
    if (rev) {
      uint32 revNr;
      if ((rev >> revNr) && revNr == nr) {
        size_t size;
        while (rev >> size) {
          cerr << "Loading file #" << files.size() << ", " << size
               << " bytes" << '\n';
          string name(TORTURE_DIR "part");
          append(name, files.size());
          files.push_back(File(name.c_str(), size, files.size()));
        }
        return;
      }
    }
    rev.close();

    cheatMrNice();
    ofstream orev(TORTURE_DIR "rev");
    static const size_t BUF_SIZE = 65536;
    byte buf[BUF_SIZE];
    orev << nr << ' ';
    while (mem < MAX_MEM) {
      // Add another file. Size usually 512<size<64k, sometimes <2M
      size_t size = 512 + rand.get(rand.get(4) == 0 ? 21 : 16);
      orev << size << ' ';
      cerr << "File #" << files.size() << " has size " << size << '\n';
      string name(TORTURE_DIR "part");
      append(name, files.size());
      bofstream o(name.c_str(), ios::binary);
      if (!o) {
        cerr << "Couldn't open " << name << " for output ("
             << strerror(errno) << ')' << endl;
        continue;
      }
      // Fill data with random bytes - or sometimes with the same byte!
      bool zeroes = (rand.get(7) == 0) && zeroFile == -1;
      if (zeroes) {
        cerr << "Will fill with zero bytes\n";
        for (size_t i = 0; i < BUF_SIZE; ++i) buf[i] = 0;
        zeroFile = files.size();
      }
      size_t sizeLeft = size;
      while (sizeLeft > 0 && o) {
        size_t n = (sizeLeft < BUF_SIZE ? sizeLeft : BUF_SIZE);
        if (!zeroes)
          for (size_t i = 0; i < n; ++i) buf[i] = rand.get(8);
        writeBytes(o, buf, n);
        if (!o) cerr << "Argh - write() failed! (" << strerror(errno)
                     << ')' << endl;
        sizeLeft -= n;
      }
      // 1 out of 8 files contains (parts of) other files
      int subparts = 0;
      if (files.size() > 1 && rand.get(3) == 0) {
        ++subparts; // likelihood of further parts decreases exponentially
        while (rand.get(1) == 0) ++subparts;
      }
      // Overwrite parts with parts of other files, but never with whole file
      int prevOtherFile = -1;
      for (int n = 0; n < subparts; ++n) {
        int otherFile;
        while ((otherFile = rand.rnd(files.size())) == prevOtherFile) { }
        size_t otherOff = (rand.get(4) == 0 ?
                           0 : rand.rnd(1 + rand.rnd(size - 256)));
        size_t otherLen =
          (size_t)rand.rnd(min(size - otherOff, files[otherFile].size) - 1);
        cerr << "Will overwrite with " << otherLen << " bytes from file #"
             << otherFile << " at offset " << otherOff << '\n';
        //cerr << (void*)files[otherFile].data << "..."
        // << (void*)(files[otherFile].data+otherLen) << endl;
        o.seekp(otherOff, ios::beg);
        writeBytes(o, files[otherFile].data, otherLen);
        if (!o) cerr << "Aargh - write() failed! (" << strerror(errno)
                     << ')' << endl;
        prevOtherFile = otherFile;
      }
      o.close();
      // Now mmap the file we just wrote
      files.push_back(File(name.c_str(), size, files.size()));
      mem += size;
    }
  }
  //______________________________________________________________________

  void mkimage(uint32 nr) {
    Rand rand(nr);
    cheatMrNice();
    bofstream img(TORTURE_DIR "image", ios::binary);
    bofstream info(TORTURE_DIR "image"EXTSEPS"info");
    size_t mem = 0;
    imageMatches.resize(0);
    while (mem < MAX_IMAGE) {
      uint32 x = rand.get(8);
      if (x < 85) {
        // an area of random data
        size_t size = static_cast<size_t>(rand.get(18));
        for (size_t i = 0; i < size; ++i)
          img << static_cast<byte>(rand.get(8) ^ 0xff);
        mem += size;
      } else if (x < 2*85) {
        // a complete file
        int i = static_cast<int>(rand.rnd(files.size()));
        info << mem << ' ' << i << '\n';
        writeBytes(img, files[i].data, files[i].size);
        imageMatches.push_back(Match(mem, i));
        debug("%1: Complete: #%2", mem, i);
        mem += files[i].size;
      } else {
        // an incomplete file (some bytes missing at the end)
        int i = static_cast<int>(rand.rnd(files.size()));
        size_t size = static_cast<size_t>(rand.rnd(files[i].size-1));
        debug("%1: Incomplete: %2 bytes from #%3", mem, size, i);
        writeBytes(img, &files[i].data[0], size);
        mem += size;
      }
    }
  }
  //______________________________________________________________________

  struct TortureReport : public MkTemplate::ProgressReporter,
                         public JigdoDesc::ProgressReporter,
                         public MD5Sum::ProgressReporter,
                         public JigdoConfig::ProgressReporter {
    virtual ~TortureReport() { }
    virtual void matchFound(const FilePart* file, uint64 offInImage) {
      size_t n = 0;
      const string& leaf = file->leafName();
      for (string::const_iterator i = leaf.begin(), e = leaf.end();
           i != e; ++i)
        if (*i >= '0' && *i <= '9') n = 10 * n + (*i - '0'); else n = 0;
      matches.push_back(Match(offInImage, n));
    }
    void info(const string&) { }
    vector<Match> matches;
  };
  //______________________________________________________________________

} // namespace
//______________________________________________________________________

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    cout << "Syntax: " << argv[0] << " <number>  to create files for "
      "one test case\n        " << argv[0] << " <number1> <number2>  "
      "to create test cases [1..2) AND RUN THEM" << endl;
    exit(2);
  }
  if (argc == 2) {
    uint32 nr = static_cast<uint32>(atol(argv[1]));
    mkfiles(nr);
    mkimage(nr);
    return 0;
  } else {
#   if CHEAT_MR_NICE
    parentPid = getpid();
#   endif
    uint32 nr1 = static_cast<uint32>(atol(argv[1]));
    uint32 nr2 = static_cast<uint32>(atol(argv[2]));
    ofstream report(TORTURE_DIR "report");
    bool returnStatus = true;
    for (uint32 tc = nr1; tc < nr2; ++tc) {
      Rand rand(tc ^ 0x1234);
      mkfiles(tc); // NB will only generate new files every 128 test cases
      cerr << "==================== TEST CASE #" << tc
           << ", " << files.size() << " files ====================" << endl;
      mkimage(tc);
      // Run MkTemplate operation over it
      // Try default parameters plus 2 other random cases
      size_t blockLen = 4096;
      size_t md5BlockLen = 128*1024U - 55;
      size_t readAmount = 128U*1024;
      for (int i = 0; i < 2; ++i) {
        cerr << "     case=" << tc << static_cast<char>('a' + i)
             << " blockLen=" << blockLen
             << " md5BlockLen=" << md5BlockLen
             << " readAmount=" << readAmount << endl;
        cheatMrNice();
        TortureReport reporter;
        bifstream image(TORTURE_DIR "image", ios::binary);
        auto_ptr<ConfigFile> cfDel(new ConfigFile());
        JigdoConfig jc(TORTURE_DIR "image"EXTSEPS"jigdo",
                       cfDel.release(), reporter);
        bofstream templ(TORTURE_DIR "image"EXTSEPS"template", ios::binary);

        RecurseDir fileNames;
        for (size_t i = 0; i < files.size(); ++i) { // Add files
          string f(TORTURE_DIR "part");
          append(f, i);
          fileNames.addFile(f);
        }
        JigdoCache cache(cacheFile, 60*60*24, readAmount);
        cache.setParams(blockLen, md5BlockLen);
        while (true) {
          try { cache.readFilenames(fileNames); }
          catch (RecurseError e) { cerr << e.message << endl; continue; }
          break;
        }
        //____________________

        // CREATE TEMPLATE
        MkTemplate op(&cache, &image, &jc, &templ, reporter, 0,
                      readAmount);
        op.run("image", "image"EXTSEPS"template");

        // Write out reported offsets for comparison with image.info
        ofstream imageReport(TORTURE_DIR "image"EXTSEPS"reported");
        for (vector<Match>::iterator i = reporter.matches.begin(),
               e = reporter.matches.end(); i != e; ++i)
          imageReport << i->off << ' ' << i->nr << endl;

        // Check whether reported offsets are correct
        /* To see the complete set of differences between the files that were
           in the image and the files that mktemplate /reported/ to be there,
           use "diff ironmaiden/image.info ironmaiden/image.reported". */
        bool offsetCheckOK = true;
        bool allChecksOK = true;
        vector<Match>::iterator j = reporter.matches.begin();
        for (vector<Match>::iterator i = imageMatches.begin(),
               e = imageMatches.end(); i != e && offsetCheckOK; ++i) {
          if (files[i->nr].size < blockLen) continue;
          if (j == reporter.matches.end()
              || i->off != j->off || i->nr != j->nr) {
            cerr << "(info: " << i->off << ' ' << i->nr << ") != (reported: "
                 << j->off << ' ' << j->nr << ')' << endl;
            offsetCheckOK = false;
            break;
          }
          ++j;
        }
        if (offsetCheckOK) {
          cerr << "OK   make-template" << endl;
        } else {
          cerr << "FAIL make-template" << endl;
          allChecksOK = false;
          returnStatus = FAILURE;
        }

        // Write jigdo
        ofstream jigdo(TORTURE_DIR "image"EXTSEPS"jigdo", ios::binary);
        jigdo << jc.configFile();

        image.close();
        jigdo.close();
        templ.close();
        //____________________

        // RE-CREATE IMAGE

        cheatMrNice();
        bifstream templIn(TORTURE_DIR "image"EXTSEPS"template", ios::binary);
        bool mkImageOK = true;
        try {
          if (JigdoDesc::makeImage(&cache,
              string(TORTURE_DIR "image"EXTSEPS"out"),
              string(TORTURE_DIR "image"EXTSEPS"tmp"),
              string(TORTURE_DIR "image"EXTSEPS"template"), &templIn,
              true, reporter, readAmount, true) > 0) mkImageOK = false;
        } catch (Error e) {
          cerr << e.message << endl;
          mkImageOK = false;
          returnStatus = FAILURE;
        }

        if (mkImageOK) {
          cerr << "OK   make-image" << endl;
        } else {
          cerr << "FAIL make-image" << endl;
          allChecksOK = false;
          returnStatus = FAILURE;
        }
        //____________________

        // VERIFY CREATED IMAGE
        bool verifyOK = false;
        JigdoDescVec contents;
        JigdoDesc::ImageInfo* info;

        templIn.close();
        cheatMrNice();
        templIn.open(TORTURE_DIR "image"EXTSEPS"template", ios::binary);
        try {
          if (JigdoDesc::isTemplate(templIn) == false)
            cerr << "not a template file?!" << endl;

          JigdoDesc::seekFromEnd(templIn);
          templIn >> contents;
          if (!templIn) cerr << "couldn't read from template" << endl;
          info = dynamic_cast<JigdoDesc::ImageInfo*>(contents.back());
          if (info == 0)
            cerr << "verify: Invalid template data - corrupted file?"
                 << endl;
        } catch (JigdoDescError e) {
          cerr << e.message << endl;
          info = 0;
        }

        MD5Sum md; // MD5Sum of image
        bifstream imageVer(TORTURE_DIR "image"EXTSEPS"out", ios::binary);
        md.updateFromStream(imageVer, info->size(), readAmount, reporter);
        md.finish();
        if (info != 0 && imageVer) {
          imageVer.get();
          if (imageVer.eof() && md == info->md5()) verifyOK = true;
        }

        if (verifyOK) {
          cerr << "OK   verify" << endl;
        } else {
          cerr << "FAIL verify" << endl;
          allChecksOK = false;
          returnStatus = FAILURE;
        }
        templIn.close();
        //____________________

        report << (allChecksOK ? "OK" : "FAIL") << " case=" << tc
               << ", blockLen=" << blockLen
               << ", md5BlockLen=" << md5BlockLen
               << ", readAmount=" << readAmount << endl;

        blockLen = 1024 + rand.get(16);
        md5BlockLen = 1024 + rand.get(18);
        if (md5BlockLen <= blockLen) md5BlockLen = blockLen + 1;
        readAmount = 16384 + rand.get(19);
      }
    }
#   if CHEAT_MR_NICE
    // Kill parent process
    if (getpid() != parentPid) {
      kill(parentPid, SIGINT);
    }
#   endif
    if (returnStatus) return 0; else return 1;
  }
}

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Find proxy URLs by reading config files of various browsers.

  Warning, this code probably tries to be too clever: It compares the "last
  modified" timestamps of the various config files and chooses the
  configuration with the most recent timestamp.

*/

// This is what libcurl says about env vars (url.c:2326):
    /* If proxy was not specified, we check for default proxy environment
     * variables, to enable i.e Lynx compliance:
     *
     * http_proxy=http://some.server.dom:port/
     * https_proxy=http://some.server.dom:port/
     * ftp_proxy=http://some.server.dom:port/
     * gopher_proxy=http://some.server.dom:port/
     * no_proxy=domain1.dom,host.domain2.dom
     *   (a comma-separated list of hosts which should
     *   not be proxied, or an asterisk to override
     *   all proxy variables)
     * all_proxy=http://some.server.dom:port/
     *   (seems to exist for the CERN www lib. Probably
     *   the first to check for.)
     *
     * For compatibility, the all-uppercase versions of these variables are
     * checked if the lowercase versions don't exist.
     */

#include <config.h>

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glibcurl.h>
#include <log.hh>
#include <proxyguess.hh>
//______________________________________________________________________

DEBUG_UNIT("proxyguess")

#ifndef TESTING_PROXYGUESS
#warning TODO glibcurl_add_proxy
void glibcurl_add_proxy(const char*, const char*) { }
void glibcurl_add_noproxy(const char*) { }
#endif

#if WINDOWS

#include <windows.h>

/* Windows: Read Internet Explorer's proxy settings. Doesn't work with .pac
   files, just with user-supplied servers. */
namespace {

  void proxyGuess_MSIE(HKEY internetSettings) {
    const unsigned BUFLEN = 256;
    DWORD len = BUFLEN;
    byte buf[BUFLEN];
    DWORD type;

    if (RegQueryValueEx(internetSettings, "ProxyEnable", NULL, &type,
                        buf, &len) == ERROR_SUCCESS
        && type == REG_BINARY && buf[0] == 0) {
      // User deselected the option "Use a proxy server", so don't continue
      debug("No proxies set up");
      return;
    }

    len = BUFLEN;
    // List of servers not to use the proxy for
    if (RegQueryValueEx(internetSettings, "ProxyOverride", NULL, &type,
                        buf, &len) == ERROR_SUCCESS
        && type == REG_SZ && buf[0] != 0) {
      // String has the form "lan;<local>". Split at ; and ignore <local>
      string host;
      const char* list = reinterpret_cast<const char*>(buf);
      while (*list != '\0') {
        if (!(isalnum(*list) || *list == '.' || *list == '-')) {
          ++list;
          continue;
        }
        while (isalnum(*list) || *list == '.' || *list == '-') {
          host += *list;
          ++list;
        }
        if (host != "local") {
          debug("No proxy for %1", host);
          glibcurl_add_noproxy(host.c_str());
        }
        host.erase();
      }
    }

    len = BUFLEN;
    if (RegQueryValueEx(internetSettings, "ProxyServer", NULL, &type,
                        buf, &len) == ERROR_SUCCESS
        && type == REG_SZ && len >= 2 && buf[0] != 0) {
      /* String has one of two formats. Either simple format, one proxy for
         all: "ox:8080", or per-protocol format:
         "ftp=ox:8081;gopher=ox:8080;http=ox:8080;https=ox:8080" */
      string entry;
      const char* list = reinterpret_cast<const char*>(buf);
      while (true) {
        while (*list != '\0' && *list != ';') {
          entry += *list;
          ++list;
        }
        string::size_type equals = entry.find('=');
        if (equals == string::npos) {
          // Simple proxy setting, assume it's both for HTTP and FTP
          string proxy = "http://";
          proxy.append(reinterpret_cast<const char*>(buf));
          debug("General proxy: %1", proxy);
          glibcurl_add_proxy("http", proxy.c_str());
          glibcurl_add_proxy("ftp", proxy.c_str());
        } else {
          // Per-protocol proxy settings
          string proto(entry, 0, equals);
          if (proto == "http" || proto == "ftp") {
            string proxy = "http://";
            proxy.append(entry, equals + 1, string::npos);
            debug("%1 proxy: %2", proto, proxy);
            glibcurl_add_proxy(proto.c_str(), proxy.c_str());
          }
        }
        entry.erase();
        if (*list == '\0') break;
        ++list;
      } // endwhile (true)
    } // endif (RegQueryValueEx("ProxyServer") == ERROR_SUCCESS)

  }
}

void proxyGuess() {
  HKEY software;
  if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software", 0, KEY_READ, &software)
      == ERROR_SUCCESS) {
    HKEY microsoft;
    if (RegOpenKeyEx(software, "Microsoft", 0, KEY_READ, &microsoft)
        == ERROR_SUCCESS) {
      HKEY windows;
      if (RegOpenKeyEx(microsoft, "Windows", 0, KEY_READ, &windows)
          == ERROR_SUCCESS) {
        HKEY currentVersion;
        if (RegOpenKeyEx(windows, "CurrentVersion", 0, KEY_READ,
                         &currentVersion) == ERROR_SUCCESS) {
          HKEY internetSettings;
          if (RegOpenKeyEx(currentVersion, "Internet Settings", 0,
                           KEY_READ, &internetSettings) == ERROR_SUCCESS) {
            proxyGuess_MSIE(internetSettings);
            RegCloseKey(internetSettings);
          }
          RegCloseKey(currentVersion);
        }
        RegCloseKey(windows);
      }
      RegCloseKey(microsoft);
    }
    RegCloseKey(software);
  }
}
//______________________________________________________________________

#else

// Proxy guess code for Unix

namespace {

  /** Local struct: Info about a browser configuration file */
  struct BrowserConfig {
    /* Init filename and timestamp */
    BrowserConfig(const string& name, time_t timestamp);
    void init(time_t timestamp);
    virtual ~BrowserConfig();
    /* Reads config file, returns true if proxies could be set. */
    virtual bool read() = 0;

    /* In order for things to work with time_t wraparound, record one recent
       timestamp "now" and always work with the "age" of files in seconds. */
    static time_t now;
    string filename;
    signed age; // Allow future dates, e.g. happens with NFS + clock skew
  };

  BrowserConfig::BrowserConfig(const string& name, time_t timestamp) {
    if (now == 0) time(&now);
    filename = name;
    age = now - timestamp;
  }

  BrowserConfig::~BrowserConfig() { }

  time_t BrowserConfig::now = 0;

  // Compare pointers to BrowserConfigs by comparing the objects' age
  struct ConfFilesCompare {
    bool operator()(const BrowserConfig* a, const BrowserConfig* b) {
      return a->age < b->age;
    }
  };

}
//______________________________________________________________________

namespace {

# ifndef TESTING_PROXYGUESS
  // Return the last modification date of the file in question, 0 on error
  inline time_t fileModTime(const char* path) {
    struct stat fileInfo;
    if (stat(path, &fileInfo) != 0) return 0;
    return fileInfo.st_mtime;
  }
  typedef ifstream MyIfstream;
# endif

  // Add file info to set of candidate config files
  template<class Browser>
  void add(set<BrowserConfig*, ConfFilesCompare>* c, const char* name) {
    if (name == 0) return;
    time_t timestamp = fileModTime(name);
    if (timestamp == 0) return;
    c->insert(new Browser(string(name), timestamp));
  }

  template<class Browser>
  void add(set<BrowserConfig*, ConfFilesCompare>* c, const string& name) {
    time_t timestamp = fileModTime(name.c_str());
    if (timestamp == 0) return;
    c->insert(new Browser(name, timestamp));
  }

  // E.g. protocol == "http", proxy == "http://localhost:8080/"
  // NB proxy must start with a scheme like "http:"
  inline bool addProxy(const char* protocol, const char* proxy) {
    while (*proxy == ' ' || *proxy == '\t' || *proxy == '=') ++proxy;
    if (*proxy == '#') return false; // Assuming comment, not setting proxy
    if (*proxy == '\0') return false;
    debug("%1 proxy: %2", protocol, proxy);
    glibcurl_add_proxy(protocol, proxy);
    return true;
  }

  void addNoProxy(const char* list) {
    string host;
    while (*list != '\0') {
      if (*list == '#') return; // Assuming start of comment
      if (!(isalnum(*list) || *list == '.' || *list == '-')) {
        ++list;
        continue;
      }
      while (isalnum(*list) || *list == '.' || *list == '-') {
        host += *list;
        ++list;
      }
      debug("No proxy for %1", host);
      glibcurl_add_noproxy(host.c_str());
      host.erase();
    }
  }

} // namespace
//______________________________________________________________________

namespace {

  struct Lynx : BrowserConfig {
    Lynx(const string& n, time_t t) : BrowserConfig(n, t) { }
    bool read();
  };

  bool Lynx::read() {
    bool result = false;
    static const string whitespace = " \t";
    MyIfstream s(filename.c_str());
    string line;
    while (s) {
      getline(s, line);
      string::size_type pos = line.find_first_not_of(whitespace);
      if (pos == string::npos) continue;
      const char* l = line.c_str();
      if (strncmp(l + pos, "http_proxy:", 11) == 0) {
        if (addProxy("http", l + 11)) result = true;
      } else if (strncmp(l + pos, "ftp_proxy:", 10) == 0) {
        if (addProxy("ftp", l + 10)) result = true;
      } else if (strncmp(l + pos, "no_proxy:", 9) == 0) {
        addNoProxy(l + 9);
      }
    }
    return result;
  }

}
//______________________________________________________________________

namespace {

  struct Wget : BrowserConfig {
    Wget(const string& n, time_t t) : BrowserConfig(n, t) { }
    bool read();
  };

  bool Wget::read() {
    // Maybe this should react on "use_proxy = off", but it doesn't
    bool result = false;
    static const string whitespace = " \t";
    MyIfstream s(filename.c_str());
    string line;
    while (s) {
      getline(s, line);
      string::size_type pos = line.find_first_not_of(whitespace);
      if (pos == string::npos) continue;
      const char* l = line.c_str();
      if (strncmp(l + pos, "http_proxy", 10) == 0) {
        if (addProxy("http", l + 10)) result = true;
      } else if (strncmp(l + pos, "ftp_proxy", 9) == 0) {
        if (addProxy("ftp", l + 9)) result = true;
      } else if (strncmp(l + pos, "no_proxy", 8) == 0) {
        addNoProxy(l + 8);
      }
    }
    return result;
  }

}
//______________________________________________________________________

namespace {

  struct Mozilla : BrowserConfig {
    Mozilla(const string& n, time_t t) : BrowserConfig(n, t) { }
    bool read();
    void setString(string* dest, const char* src);
  };

  void Mozilla::setString(string* dest, const char* src) {
    dest->erase();
    while (*src == '"' || *src == ' ' || *src == '\t' || *src == ',') ++src;
    while (*src != '\0' && *src != ')' && *src != '"') {
      *dest += *src;
      ++src;
    }
  }

  // Works for Netscape 4.7, Galeon, Mozilla 5/6
  bool Mozilla::read() {
    // Doesn't understand .pac proxy definitions
    MyIfstream s(filename.c_str());
    string line, ftphost, ftpport, httphost, httpport, noproxy;
    while (s) {
      getline(s, line);
      const char USERPREF[] = "user_pref(\"network.proxy.";
      const char PREF[] = "pref(\"network.proxy.";
      const char* l = line.c_str();
      if (strncmp(l, USERPREF, sizeof(USERPREF)-1) == 0)
        l += sizeof(USERPREF)-1;
      else if (strncmp(l, PREF, sizeof(PREF)-1) == 0)
        l += sizeof(PREF)-1;
      else
        continue;
      if (strncmp(l, "ftp\"", 4) == 0)
        setString(&ftphost, l + 4);
      else if (strncmp(l, "ftp_port\"", 9) == 0)
        setString(&ftpport, l + 9);
      else if (strncmp(l, "http\"", 5) == 0)
        setString(&httphost, l + 5);
      else if (strncmp(l, "http_port\"", 10) == 0)
        setString(&httpport, l + 10);
      else if (strncmp(l, "no_proxies_on\"", 14) == 0)
        setString(&noproxy, l + 14);
    }

    bool result = false;
    if (!httphost.empty() && !httpport.empty()) {
      httphost.insert(0, "http://");
      httphost += ':';
      httphost += httpport;
      if (addProxy("http", httphost.c_str())) result = true;
    }
    if (!ftphost.empty() && !ftpport.empty()) {
      ftphost.insert(0, "http://");
      ftphost += ':';
      ftphost += ftpport;
      if (addProxy("ftp", ftphost.c_str())) result = true;
    }
    if (!noproxy.empty()) addNoProxy(noproxy.c_str());
    return result;
  }

}
//______________________________________________________________________

namespace {

  struct KDE : BrowserConfig {
    KDE(const string& n, time_t t) : BrowserConfig(n, t) { }
    bool read();
  };

  // Reads kioslaverc
  bool KDE::read() {
    // Maybe this should react on "use_proxy = off", but it doesn't
    bool result = false;
    static const string whitespace = " \t";
    MyIfstream s(filename.c_str());
    string line;
    while (s) {
      getline(s, line);
      string::size_type pos = line.find_first_not_of(whitespace);
      if (pos == string::npos) continue;
      const char* l = line.c_str();
      if (strncmp(l + pos, "httpProxy", 9) == 0) {
        if (addProxy("http", l + 9)) result = true;
      } else if (strncmp(l + pos, "ftpProxy", 8) == 0) {
        if (addProxy("ftp", l + 8)) result = true;
      } else if (strncmp(l + pos, "NoProxyFor", 10) == 0) {
        addNoProxy(l + 10);
      }
    }
    return result;
  }

}
//______________________________________________________________________

void proxyGuess() {
  string home = g_get_home_dir();
  if (home[home.size() - 1] != DIRSEP) home += DIRSEP;
  //____________________

  typedef set<BrowserConfig*, ConfFilesCompare> ConfFiles;
  ConfFiles c;

  add<Lynx>(&c, "/etc/lynx.cfg");
  add<Lynx>(&c, home + ".lynxrc");
  add<Lynx>(&c, getenv("LYNX_CFG"));

  add<Wget>(&c, "/etc/wgetrc");
  add<Wget>(&c, home + ".wgetrc");
  add<Wget>(&c, getenv("WGETRC"));

  add<Mozilla>(&c, "/etc/netscape4/defaults/preferences.js");
  add<Mozilla>(&c, "/etc/mozilla/prefs.js");
  add<Mozilla>(&c, home + ".netscape/preferences.js");
  add<Mozilla>(&c, home + ".galeon/mozilla/galeon/prefs.js");
  add<Mozilla>(&c, home + ".mozilla/default/prefs.js");
  add<Mozilla>(&c, home + ".netscape6/default/prefs.js"); // Is this correct?

  add<KDE>(&c, home + ".kde/share/config/kioslaverc");

  bool finished = false;
  while (!finished && !c.empty()) {
    ConfFiles::iterator first = c.begin();
    ConfFiles::iterator second = first;
    ++second;
    BrowserConfig* bc = *first;
    debug("Scan: %1  %2", bc->age, bc->filename);
    finished = bc->read();
    delete bc;
    c.erase(first, second);
  }
  // Most recent browser config was found, just delete rest
  while (!c.empty()) {
    ConfFiles::iterator first = c.begin();
    ConfFiles::iterator second = first;
    ++second;
    BrowserConfig* bc = *first;
    debug("Ignr: %1  %2", bc->age, bc->filename);
    delete bc;
    c.erase(first, second);
  }

}

#endif /* WINDOWS */

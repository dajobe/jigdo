/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Redirects proxyguess's file accesses to in-memory stringstreams

  #test-deps

*/

#include <config.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <glib.h>

#include <debug.hh>
#include <log.hh>

#define glibcurl_add_noproxy glibcurl_add_noproxy_ORIG
#define glibcurl_add_proxy glibcurl_add_proxy_ORIG
#include <glibcurl.h>
#undef glibcurl_add_noproxy
#undef glibcurl_add_proxy
//______________________________________________________________________

#if WINDOWS

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);

  Assert(false); // Test unimplemented
  return 0;
}
//______________________________________________________________________

#else

namespace {

  time_t start = time(0);

  struct FakeFile {
    FakeFile(time_t s, const string& c) : content(c), stamp(s) { }
    string content;
    time_t stamp;
  };

  // Mapping path -> file obj
  typedef map<string,FakeFile*> Map;
  Map files;

  string proxySettings;
  void testcaseStart() {
    proxySettings.clear();
    files.clear();
  }

  inline void addFile(const char* path, FakeFile* f) {
    files.insert(files.end(), make_pair<string,FakeFile*>(path, f));
  }
  inline void addFile(const char* path, auto_ptr<FakeFile>& f) {
    files.insert(files.end(), make_pair<string,FakeFile*>(path, f.get()));
  }

  // This is used in place of normal ifstream
  struct MyIfstream : istringstream {
    MyIfstream(const char* path)
      : istringstream(files.find(path) != files.end()
                      ? files[path]->content
                      : string()) { }
  };

  // Return the last modification date of the file in question, 0 on error
  inline time_t fileModTime(const char* path) {
    Map::const_iterator i = files.find(path);
    if (i == files.end()) return 0; else return i->second->stamp;
  }

}

void glibcurl_add_proxy(const gchar *protocol, const gchar *proxy) {
  if (!proxySettings.empty()) proxySettings += ',';
  proxySettings += protocol;
  proxySettings += '=';
  proxySettings += proxy;
}
void glibcurl_add_noproxy(const gchar *host) {
  if (!proxySettings.empty()) proxySettings += ',';
  proxySettings += "noproxy=";
  proxySettings += host;
}
//______________________________________________________________________

#define TESTING_PROXYGUESS
#include <proxyguess.cc>
//______________________________________________________________________

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);

  string home = g_get_home_dir();
  if (home[home.size() - 1] != DIRSEP) home += DIRSEP;

  // Set up a couple of simulated files
  auto_ptr<FakeFile> lynxNone(new FakeFile(start - 1,
    "# lynx.cfg file.\n"
    "\n"
    ".h1 Auxiliary Facilities\n"
    ".ex\n"
    "#INCLUDE:~/lynx.cfg:COLOR VIEWER KEYMAP\n"
    "STARTFILE:file://localhost/usr/share/doc/lynx/lynx_help/lynx_help_main.html\n"
    "#\n"
    "HELPFILE:file://localhost/usr/share/doc/lynx/lynx_help/lynx_help_main.html\n"
    ".ex\n"
    ".nf\n"
  ));

  auto_ptr<FakeFile> lynxAll(new FakeFile(start - 10,
    "# preserve lowercasing, and will outlive the Lynx image.\n"
    "#\n"
    ".ex 15\n"
    "http_proxy:http://1.com:port/\n"
    "https_proxy:http://2.com:port/\n"
    "ftp_proxy:http://3.com:port/\n"
    "gopher_proxy:http://4.com:port/\n"
    "news_proxy:http://5.com:port/\n"
    "newspost_proxy:http://6.com:port/\n"
    "newsreply_proxy:http://7.com:port/\n"
    "snews_proxy:http://8.com:port/\n"
    "snewspost_proxy:http://9.com:port/\n"
    "snewsreply_proxy:http://10.com:port/\n"
    "nntp_proxy:http://11.com:port/\n"
    "wais_proxy:http://12.com:port/\n"
    "finger_proxy:http://13.com:port/\n"
    "cso_proxy:http://14.com:port/\n"
    "no_proxy:host.15.com,foo,bar #baz\n"
    "\n"
    "\n"
    ".h2 NO_PROXY\n"
    "# The no_proxy variable can be a comma-separated list of strings defining\n"
    "# no-proxy zones in the DNS domain name space.  If a tail substring of the\n"
    "# domain-path for a host matches one of these strings, transactions with that\n"
    "# node will not be proxied.\n"
    ".ex\n"
    "no_proxy:domain.path1,path2\n"
    "#\n"
    "# A single asterisk as an entry will override all proxy variables and no\n"
    "# transactions will be proxied.\n"
    ".ex\n"
    "no_proxy:*\n" // Is ignored completely (incorrectly)
    "# This is the only allowed use of * in no_proxy.\n"
  ));

  auto_ptr<FakeFile> wget(new FakeFile(start - 9,
    "# You can set up other headers, like Accept-Language.  Accept-Language\n"
    "# is *not* sent by default.\n"
    "header = Accept-Language: en\n"
    "\n"
    "# You can set the default proxies for Wget to use for http and ftp.\n"
    "# They will override the value in the environment.\n"
    "http_proxy = http://proxy.yoyodyne.com:18022/\n"
    "ftp_proxy=http://proxy.yoyodyne.com:18023/\n"
    "\n"
    "# If you do not want to use proxy at all, set this to off.\n"
    "use_proxy = off\n" // Incorrectly ignored
    "\n"
    "# You can customize the retrieval outlook.  Valid options are default,\n"
    "# binary, mega and micro.\n"
    "#dot_style = default\n"
  ));

  auto_ptr<FakeFile> netscape4(new FakeFile(start - 10,
    "// Netscape User Preferences\n"
    "// This is a generated file!  Do not edit.\n"
    "\n"
    "user_pref(\"mail.signature_file\", \"/home/richard/.signature\");\n"
    "user_pref(\"mail.thread.win_height\", 0);\n"
    "user_pref(\"mail.thread.win_width\", 0);\n"
    "user_pref(\"mail.use_movemail\", false);\n"
    "user_pref(\"network.cookie.cookieBehavior\", 1);\n"
    "user_pref(\"network.cookie.warnAboutCookies\", true);\n"
    "user_pref(\"network.hosts.socks_serverport\", 0);\n"
    "user_pref(\"network.proxy.ftp\", \"localhost\");\n"
    "user_pref(\"network.proxy.ftp_port\", 8080);\n"
    "user_pref(\"network.proxy.http\", \"localhost\");\n"
    "user_pref(\"network.proxy.http_port\", 5865);\n"
    "user_pref(\"network.proxy.no_proxies_on\", \"lan localhost\");\n"
    "user_pref(\"network.proxy.type\", 1);\n"
    "user_pref(\"news.default_fcc\", \"/home/richard/nsmail/Sent\");\n"
  ));

  auto_ptr<FakeFile> kde(new FakeFile(start - 10,
    "[Proxy Settings]\n"
    "NoProxyFor=lan localhost\n"
    "Proxy Config Script=\n" // Not supported
    "ProxyType=1\n"
    "ftpProxy=http://localhost1:8080\n"
    "httpProxy=http://localhost2:5865\n"
    "httpsProxy=http://localhost3:8080\n"
  ));

  auto_ptr<FakeFile> galeon(new FakeFile(start - 10,
    "# Mozilla User Preferences\n"
    "// This is a generated file!\n"
    "user_pref(\"network.http.proxy.keep-alive\", false);\n"
    "user_pref(\"network.proxy.ftp\", \"x\");\n"
    "user_pref(\"network.proxy.ftp_port\", 8080);\n"
    "user_pref(\"network.proxy.http\", \"x\");\n"
    "user_pref(\"network.proxy.http_port\", 5865);\n"
    "user_pref(\"network.proxy.no_proxies_on\", \"lan x 127.0.0.1 nenya nenya.lan\");\n"
    "user_pref(\"network.proxy.socks_version\", 4);\n"
    "user_pref(\"network.proxy.ssl_port\", 8080);\n"
    "user_pref(\"network.proxy.type\", 1);\n"
    "user_pref(\"plugin.soname.list\", \"libXt.so:libXext.so\");\n"
    "user_pref(\"security.checkloaduri\", false);\n"
    "user_pref(\"security.warn_submit_insecure\", false);\n"
  ));

  auto_ptr<FakeFile> mozillaEmpty(new FakeFile(start - 5,
    "// TryeType\n"
    "pref(\"font.FreeType2.enable\", true);\n"
    "pref(\"font.freetype2.shared-library\", \"libfreetype.so.6\");\n"
    "pref(\"font.FreeType2.autohinted\", false);\n"
    "pref(\"font.FreeType2.unhinted\", false);\n"
    "pref(\"font.antialias.min\",        10);\n"
    "pref(\"font.directory.truetype.1\", \"/var/lib/defoma/x-ttcidfont-conf.d/dirs/TrueType\");\n"
    "pref(\"font.directory.truetype.2\", \"/usr/share/fonts/truetype\");\n"
    "pref(\"font.directory.truetype.3\", \"/usr/share/fonts/truetype/openoffice\");\n"
  ));
  //____________________

  // Scan single files

  testcaseStart();
  addFile("/etc/lynx.cfg", lynxNone);
  proxyGuess();
  Assert(proxySettings == "");

  testcaseStart();
  string lynxRc = home + ".lynxrc";
  addFile(lynxRc.c_str(), lynxAll);
  proxyGuess();
  Assert(proxySettings == "http=http://1.com:port/,ftp=http://3.com:port/,"
         "noproxy=host.15.com,noproxy=foo,noproxy=bar,noproxy=domain.path1,"
         "noproxy=path2");

  testcaseStart();
  addFile("/etc/wgetrc", wget);
  proxyGuess();
  Assert(proxySettings == "http=http://proxy.yoyodyne.com:18022/,"
         "ftp=http://proxy.yoyodyne.com:18023/");

  testcaseStart();
  addFile("/etc/netscape4/defaults/preferences.js", netscape4);
  proxyGuess();
  Assert(proxySettings == "http=http://localhost:5865,"
         "ftp=http://localhost:8080,noproxy=lan,noproxy=localhost");

  testcaseStart();
  string kdeRc = home + ".kde/share/config/kioslaverc";
  addFile(kdeRc.c_str(), kde);
  proxyGuess();
  Assert(proxySettings == "noproxy=lan,noproxy=localhost,"
         "ftp=http://localhost1:8080,http=http://localhost2:5865");

  testcaseStart();
  string galeonRc = home + ".galeon/mozilla/galeon/prefs.js";
  addFile(galeonRc.c_str(), galeon);
  proxyGuess();
  Assert(proxySettings == "http=http://x:5865,ftp=http://x:8080,noproxy=lan,"
         "noproxy=x,noproxy=127.0.0.1,noproxy=nenya,noproxy=nenya.lan");

  testcaseStart();
  string mozRc = home + ".mozilla/default/prefs.js";
  addFile(mozRc.c_str(), mozillaEmpty);
  proxyGuess();
  Assert(proxySettings == "");
  //____________________

  // Scan several files, use most recent one which is not empty
  // general:        scan several
  // proxyguess:     Scan: 1  /etc/lynx.cfg
  // proxyguess:     Scan: 5  /home/richard/.mozilla/default/prefs.js
  // proxyguess:     Scan: 9  /etc/wgetrc
  // proxyguess:     http proxy: http://proxy.yoyodyne.com:18022/
  // proxyguess:     ftp proxy: http://proxy.yoyodyne.com:18023/
  // proxyguess:     Ignr: 10  /home/richard/.lynxrc
  // general:        "http=http://proxy.yoyodyne.com:18022/,ftp=http://proxy.yoyodyne.com:18023/"
  msg("scan several");
  testcaseStart();
  addFile("/etc/lynx.cfg", lynxNone);
  addFile(lynxRc.c_str(), lynxAll);
  addFile("/etc/wgetrc", wget);
  addFile("/etc/netscape4/defaults/preferences.js", netscape4);
  addFile(kdeRc.c_str(), kde);
  addFile(galeonRc.c_str(), galeon);
  addFile(mozRc.c_str(), mozillaEmpty);
  proxyGuess();
  msg("\"%1\"", proxySettings);
  Assert(proxySettings == "http=http://proxy.yoyodyne.com:18022/,"
         "ftp=http://proxy.yoyodyne.com:18023/");

  return 0;
}

#endif /* WINDOWS */

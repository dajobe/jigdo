/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download data from URL, write to output function, report on progress

  This is the one and only file which accesses libcurl directly.

*/

#include <config.h>

#include <iostream>

#include <glib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_UNAME
#  include <sys/utsname.h>
#endif
#include <curl/curl.h>

#include <debug.hh>
#include <download.hh>
#include <glibcurl.h>
#include <log.hh>
#include <string-utf.hh>
//______________________________________________________________________

string Download::userAgent;

DEBUG_UNIT("download")

namespace {

  Logger curlDebug("curl");

}

// CURLSH* Download::shHandle = 0;

// Initialize (g)libwww
void Download::init() {
  glibcurl_init();
//   shHandle = curl_share_init();
//   curl_share_setopt(shHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  glibcurl_set_callback(glibcurlCallback, 0);

  if (userAgent.empty()) {
    userAgent = "jigdo/" JIGDO_VERSION;
#   if WINDOWS
    userAgent += " (Windows";
    OSVERSIONINFO info;
    memset(&info, 0, sizeof(OSVERSIONINFO));
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&info) != 0) {
      const char* s = "";
      if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) { // 95/98/Me
        if (info.dwMinorVersion < 10) s = " 95";
        else if (info.dwMinorVersion < 90) s = " 98";
        else s = " Me";
      } else if (info.dwPlatformId == VER_PLATFORM_WIN32_NT) { // NT/00/XP/03
        if (info.dwMajorVersion < 5) s = " NT";
        else if (info.dwMinorVersion == 0) s = " 2000";
        else if (info.dwMinorVersion == 1) s = " XP";
        else if (info.dwMinorVersion == 2) s = " 2003";
        else s = " >2003";
      }
      userAgent += s;
    }
    userAgent += ')';
#   elif HAVE_UNAME
    struct utsname ubuf;
    if (uname(&ubuf) == 0) {
      userAgent += " (";
      userAgent += ubuf.sysname; userAgent += ' '; userAgent += ubuf.release;
      userAgent += ')';
    }
#   endif
    userAgent += " (+http://atterer.net/jigdo/) ";
    //userAgent += curl_version();
    const char* p = curl_version();
    while (*p != ' ' && *p != '\0') userAgent += *p++;
    userAgent += '\0';
    debug("User-Agent: %1", userAgent);
  }
}

void Download::cleanup() {
  debug("cleanup");
  // Dumps core when called - wish I knew why:
  //curl_share_cleanup(shHandle);
  glibcurl_cleanup();
}
//______________________________________________________________________

void Download::uriJoin(string* dest, const string& /*base*/, const string& rel) {
#warning uriJoin
  *dest = rel;
//   if (HTURL_isAbsolute(rel.c_str())) {
//     *dest = rel;
//   } else {
//     char* joined = HTParse(rel.c_str(), base.c_str(), PARSE_ALL);
//     *dest = HTSimplify(&joined);
//     HT_FREE(joined);
//   }
}
//______________________________________________________________________

Download::Download(const string& uri, Output* o)
    : handle(0), uriVal(uri), resumeOffsetVal(0), currentSize(0),
      outputVal(o), state(CREATED), stopLaterId(0), insideNewData(false) {
  /* string::data() just points at the "raw" memory that contains the string
     data. In contrast, string::c_str() may create a temporary buffer, add
     the null byte, and destroy that buffer during the next method invocation
     on that string. Since libcurl needs access to the URL all the time,
     using c_str() seems unwise/unportable. Add terminator to data()
     instead. */
  uriVal += '\0';
}
//________________________________________

Download::~Download() {
  debug("~Download %1", uriVal.data());
  Assert(!insideNewData);
  //   stop();
  if (handle != 0) {
    glibcurl_remove(handle);
    debug("~Download: curl_easy_cleanup(%1)", (void*)handle);
    curl_easy_cleanup(handle);
  }
  if (stopLaterId != 0) g_source_remove(stopLaterId);
}
//______________________________________________________________________

// void Download::setPragmaNoCache(bool pragmaNoCache) {
//   Paranoid(state == CREATED || failed() || succeeded() || interrupted());
//   // Force reload from originating server, bypassing proxies?
//   if (pragmaNoCache)
//     HTRequest_addGnHd(request, HT_G_PRAGMA_NO_CACHE);
//   else
//     HTRequest_setGnHd(request, static_cast<HTGnHd>(HTRequest_gnHd(request)
//                                                    & ~HT_G_PRAGMA_NO_CACHE));
// }

/* Important: The handle can be used several times - we must ensure that any
   non-default settings (e.g. "Range" header) are reset before reusing it. */
void Download::run() {
  debug("run resumeOffset=%1", resumeOffset());
  Assert(outputVal != 0); // Must have set up output

  if (handle == 0) {
    handle = curl_easy_init();
    Assert(handle != 0);

    // Set URL
    curl_easy_setopt(handle, CURLOPT_URL, uriVal.data());

    if (curlDebug) {
      // Enable debug output
      curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);
    }
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1);
    // Follow redirects
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1);
    // Enable referer for HTTP redirs (CURLOPT_REFERER to be set by creator)
    curl_easy_setopt(handle, CURLOPT_AUTOREFERER, 1);
    // Set user agent
    curl_easy_setopt(handle, CURLOPT_USERAGENT, userAgent.data());

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curlWriter);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(handle, CURLOPT_PRIVATE, this);

    //curl_easy_setopt(handle, CURLOPT_SHARE, shHandle);

    glibcurl_add(handle);
  }

  //Paranoid(handle != 0); // Don't call this after stop()
  state = RUNNING;

  // Shall we resume the download from a certain offset?
  currentSize = resumeOffset();
  curl_easy_setopt(handle, CURLOPT_RESUME_FROM_LARGE, resumeOffset());

  // TODO: CURLOPT_PROXY*

  return;
}
//______________________________________________________________________

size_t Download::curlWriter(void* data, size_t size, size_t nmemb,
                            void* selfPtr) {
  Download* self = static_cast<Download*>(selfPtr);
  unsigned len = size * nmemb;

  if (self->stopLaterId != 0) return len;
  self->insideNewData = true;
  //if (self->state == PAUSE_SCHEDULED) self->pauseNow();
  self->currentSize += len;
  self->outputVal->download_data(reinterpret_cast<const byte*>(data),
                                 len, self->currentSize);
  self->insideNewData = false;
  return len;
}
//______________________________________________________________________

#if 0
// Function which is called by libwww whenever anything happens for a request
BOOL Download::alertCallback(HTRequest* request, HTAlertOpcode op,
                             int /*msgnum*/, const char* /*dfault*/,
                             void* input, HTAlertPar* /*reply*/) {
  if (request == 0) return NO;
  // A Download object hides behind the output stream registered with libwww
  Download* self = getDownload(request);

  /* If state==ERROR, then output->error() has already been called - don't
     send further info. */
  if (self->state == ERROR) return YES;

  char* host = "host";
  if (input != 0) host = static_cast<char*>(input);

  if (op != HT_PROG_READ)
    debug("Alert %1 for %2 obj %3", op, self->uri(), self);

  string info;
  switch (op) {
  case HT_PROG_DNS:
    info = subst(_("Looking up %L1"), host);
    self->outputVal->download_message(&info);
    break;
  case HT_PROG_CONNECT:
    info = subst(_("Contacting %L1"), host);
    self->outputVal->download_message(&info);
    break;
  case HT_PROG_LOGIN:
    info = _("Logging in");
    self->outputVal->download_message(&info);
    break;
  case HT_PROG_READ: {
    // This used to be here. It doesn't work with 206 Partial Content
    //long len = HTAnchor_length(HTRequest_anchor(request));
    // This one is better
    HTResponse* response = HTRequest_response(request);
    long len = -1;
    if (response != 0) len = HTResponse_length(response);
    if (len != -1 && static_cast<uint64>(len) != self->currentSize)
      self->outputVal->download_dataSize(self->resumeOffset() + len);
    break;
  }
  default:
    break;
  }

  return YES; // Value only relevant for op == HT_A_CONFIRM
}
#endif
//______________________________________________________________________

void Download::glibcurlCallback(void*) {
  int inQueue;
  while (true) {
    CURLMsg* msg = curl_multi_info_read(glibcurl_handle(), &inQueue);
    if (msg == 0) break;
    char* privatePtr;
    curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &privatePtr);
    Download* download = reinterpret_cast<Download*>(privatePtr);
    if (msg->msg == CURLMSG_DONE) {
      debug("glibcurlCallback: Download %1 done, code %2", download,
            msg->data.result);
      download->state = SUCCEEDED;
      download->outputVal->download_succeeded();
    } else {
      debug("glibcurlCallback: Download %1 not done, code %2", download,
            msg->data.result);
    }
  }
}

#if 0
namespace {
  struct libwwwError { int code; const char* msg; const char* type; };
  libwwwError libwwwErrors[] = { HTERR_ENGLISH_INITIALIZER };
}

int Download::afterFilter(HTRequest* request, HTResponse* /*response*/,
                          void* /*param*/, int status) {
  Download* self = getDownload(request);

#if DEBUG
  const char* msg = "";
  switch (status) {
  case HT_ERROR: msg = " (HT_ERROR)"; break;
  case HT_LOADED: msg = " (HT_LOADED)"; break;
  case HT_PARTIAL_CONTENT: msg = " (HT_PARTIAL_CONTENT)"; break;
  case HT_NO_DATA: msg = " (HT_NO_DATA)"; break;
  case HT_NO_ACCESS: msg = " (HT_NO_ACCESS)"; break;
  case HT_NO_PROXY_ACCESS: msg = " (HT_NO_PROXY_ACCESS)"; break;
  case HT_RETRY: msg = " (HT_RETRY)"; break;
  case HT_PERM_REDIRECT: msg = " (HT_PERM_REDIRECT)"; break;
  case HT_TEMP_REDIRECT: msg = " (HT_TEMP_REDIRECT)"; break;
  }
  debug("Status %1%2 for %L3 obj %4", status, msg, self->uri(), self);
#endif

  // Download finished, or server dropped connection on us
  if (status >= 0) {
    HTResponse* response = HTRequest_response(request);
    long len = -1;
    if (response != 0) len = HTResponse_length(response);
    if (len == -1 || (len + self->resumeOffset()) == self->currentSize) {
      // Download finished
      self->state = SUCCEEDED;
      self->outputVal->download_succeeded();
      return HT_OK;
    }
  }

  // The connection dropped or there was a timeout
  /* libwww returns just -1 (HT_ERROR) if I tear down the HTTP connection
     very early, at a guess before the headers are transmitted completely.
     Might want to add -1 in the if() below, but that's a very generic error
     code... :-/ */
  if (status >= 0 || status == HT_INTERRUPTED || status == HT_TIMEOUT) {
    self->generateError(INTERRUPTED);
    return HT_OK;
  }

  self->generateError();
  return HT_OK;
}
#endif
//______________________________________________________________________

/* Pause download by removing the request's socket from the list of sockets
   to call select() with. */
// void Download::pauseNow() {
//   Paranoid(state == PAUSE_SCHEDULED);
//   state = PAUSED;
//   if (handle == 0) return;
//   Assert(glibcurl_remove(handle) == CURLM_OK);
//   debug("Download::pauseNow");
// }

void Download::pause() {
  Assert(!insideNewData);
  state = PAUSED;
  if (handle == 0) return;
  Assert(glibcurl_remove(handle) == CURLM_OK);
  debug("Download::pause");
}

// Analogous to pauseNow() above
void Download::cont() {

  /* Broken ATM: libcurl will go
* Connection 0 seems to be dead!
* Closing connection #0
* About to connect() to 127.0.0.1 port 8000
* Connected to localhost (127.0.0.1) port 8000
... and then restart the transfer, but without the right range header */
  Assert(false);

  //if (state == PAUSE_SCHEDULED) state = RUNNING;
  if (state == RUNNING) return;
  Assert(paused());
  state = RUNNING;
  if (handle == 0) return;
  glibcurl_add(handle);
  debug("Download::cont");
}
//______________________________________________________________________

void Download::stop() {
  if (handle == 0) return;
  if (state == ERROR || state == INTERRUPTED || state == SUCCEEDED) return;
  state = INTERRUPTED;//ERROR;//SUCCEEDED;

  if (insideNewData) {
    debug("stop later");
    // Cannot call curl_easy_cleanup() (segfaults), so do it later
    if (stopLaterId != 0) return;
    stopLaterId = g_idle_add_full(G_PRIORITY_HIGH_IDLE, &stopLater_callback,
                                  (gpointer)this, NULL);
    Assert(stopLaterId != 0); // because we use 0 as a special value
  } else {
    debug("stop now: curl_easy_cleanup(%1)", (void*)handle);
    glibcurl_remove(handle);
    curl_easy_cleanup(handle);
    handle = 0;
  }

  // None of this is really the right thing. Believe me, I tried both. ;-/
  //string err = _("Download stopped");
  //outputVal->download_failed(&err);
  //outputVal->download_succeeded();
}

namespace {

  const char* curlStrErr(CURLcode cc) {
    switch (cc) {
    CURLE_OK: return _("No error");
    CURLE_UNSUPPORTED_PROTOCOL: return _("Unsupported protocol");
    CURLE_URL_MALFORMAT: return _("The URL was not properly formatted");
    CURLE_URL_MALFORMAT_USER: return _("The user part of the URL syntax was not correct");
    CURLE_COULDNT_RESOLVE_PROXY: return _("The given proxy host could not be resolved");
    CURLE_COULDNT_RESOLVE_HOST: return _("The given remote host could not be resolved");
    CURLE_COULDNT_CONNECT: return _("Failed to connect to host or proxy");
    CURLE_FTP_WEIRD_SERVER_REPLY: return "Weird FTP server reply";
    CURLE_FTP_ACCESS_DENIED: return _("Access denied on FTP server");
    CURLE_FTP_USER_PASSWORD_INCORRECT: return _("Incorrect username and/or password");
    CURLE_FTP_WEIRD_PASS_REPLY: return "Weird FTP server reply to PASS";
    CURLE_FTP_WEIRD_USER_REPLY: return "Weird FTP server reply to USER";
    CURLE_FTP_WEIRD_PASV_REPLY: return "Weird FTP server reply to PASV";
    CURLE_FTP_WEIRD_227_FORMAT: return "Weird FTP server `227' reply";
    CURLE_FTP_CANT_GET_HOST: return "Failure to lookup the host used for the new FTP connection";
    CURLE_FTP_CANT_RECONNECT: return "Cannot reconnect to FTP server after weird PASV or EPSV answer";
    CURLE_FTP_COULDNT_SET_BINARY: return _("Error when trying to set FTP transfer mode to binary");
    CURLE_PARTIAL_FILE: return _("Transfer interrupted");
    CURLE_FTP_COULDNT_RETR_FILE: return "Weird FTP server reply to RETR";
    CURLE_FTP_WRITE_ERROR: return "No `transfer successful' after FTP upload";
    CURLE_FTP_QUOTE_ERROR: return "Error after FTP QUOTE command";
    CURLE_HTTP_RETURNED_ERROR: return _("HTTP error");
    CURLE_WRITE_ERROR: return "Network write error";
    CURLE_MALFORMAT_USER: return "Malformat user";
    CURLE_FTP_COULDNT_STOR_FILE: return "FTP server denied the STOR operation";
    CURLE_READ_ERROR: return _("Network read error");
    CURLE_OUT_OF_MEMORY: return "Out of memory";
    CURLE_OPERATION_TIMEOUTED: return _("Timeout");
    CURLE_FTP_COULDNT_SET_ASCII: return _("Error when trying to set FTP transfer mode to ASCII");
    CURLE_FTP_PORT_FAILED: return _("FTP PORT command failed");
    CURLE_FTP_COULDNT_USE_REST: return _("Resuming of downloads unsupported");
    CURLE_FTP_COULDNT_GET_SIZE: return _("FTP server does not support file size command");
    CURLE_HTTP_RANGE_ERROR: return _("Resuming of downloads unsupported");
    CURLE_HTTP_POST_ERROR: return "HTTP POST error";
    CURLE_SSL_CONNECT_ERROR: return _("");
    CURLE_BAD_DOWNLOAD_RESUME: return _("");
    CURLE_FILE_COULDNT_READ_FILE: return _("");
    CURLE_LDAP_CANNOT_BIND: return _("");
    CURLE_LDAP_SEARCH_FAILED: return _("");
    CURLE_LIBRARY_NOT_FOUND: return _("");
    CURLE_FUNCTION_NOT_FOUND: return _("");
    CURLE_ABORTED_BY_CALLBACK: return _("");
    CURLE_BAD_FUNCTION_ARGUMENT: return _("");
    CURLE_BAD_CALLING_ORDER: return _("");
    CURLE_HTTP_PORT_FAILED: return _("");
    CURLE_BAD_PASSWORD_ENTERED: return _("");
    CURLE_TOO_MANY_REDIRECTS : return _("");
    CURLE_UNKNOWN_TELNET_OPTION: return _("");
    CURLE_TELNET_OPTION_SYNTAX : return _("");
    CURLE_OBSOLETE: return _("");
    CURLE_SSL_PEER_CERTIFICATE: return _("");
    CURLE_GOT_NOTHING: return _("");
    CURLE_SSL_ENGINE_NOTFOUND: return _("");
    CURLE_SSL_ENGINE_SETFAILED: return _("");
    CURLE_SEND_ERROR: return _("");
    CURLE_RECV_ERROR: return _("");
    CURLE_SHARE_IN_USE: return _("");
    CURLE_SSL_CERTPROBLEM: return _("");
    CURLE_SSL_CIPHER: return _("");
    CURLE_SSL_CACERT: return _("");
    CURLE_BAD_CONTENT_ENCODING: return _("");
    CURLE_LDAP_INVALID_URL: return _("");
    CURLE_FILESIZE_EXCEEDED: return _("");
    CURLE_FTP_SSL_FAILED: return _("");
    default: return _("Unknown network error");
    }
  }

}

gboolean Download::stopLater_callback(gpointer data) {
  Download* self = static_cast<Download*>(data);
  Assert(!self->insideNewData);

  debug("stopLater_callback: curl_easy_cleanup(%1)", (void*)self->handle);
  if (self->handle != 0) {
    glibcurl_remove(self->handle);
    curl_easy_cleanup(self->handle);
    self->handle = 0;
  }
  self->stopLaterId = 0;
  return FALSE; // "Don't call me again"
}
//______________________________________________________________________

#if 0
// Call output->error() with appropriate string taken from request object
/* If this is called, the Download is assumed to have failed in a
   non-recoverable way. */
void Download::generateError(State newState) {
  if (state == ERROR || state == INTERRUPTED || state == SUCCEEDED) return;

  Assert(request != 0);
  HTList* errList = HTRequest_error(request);
  HTError* err;
  int errIndex = 0;
  while ((err = static_cast<HTError*>(HTList_removeFirstObject(errList)))) {
    errIndex = HTError_index(err);
    debug("  %L1 %L2",
          libwwwErrors[errIndex].code, libwwwErrors[errIndex].msg);
  }

  string s;
  if (strcmp("client_error", libwwwErrors[errIndex].type) == 0
      || strcmp("server_error", libwwwErrors[errIndex].type) == 0) {
    // Include error code with HTTP errors
    append(s, libwwwErrors[errIndex].code);
    s += ' ';
  }
  s += libwwwErrors[errIndex].msg;

  state = newState;
  /* libwww is not internationalized, so the string always ought to be UTF-8.
     Oh well, check just to be sure. */
  bool validUtf8 = g_utf8_validate(s.c_str(), s.length(), NULL);
  Assert(validUtf8);
  if (!validUtf8)
    s = _("Error");
  outputVal->download_failed(&s);
}
#endif

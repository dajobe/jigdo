/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download data from URL, write to output function, report on progress

  This is the one and only file which accesses libwww directly.

*/

#include <config.h>

#include <iostream>

#include <glib.h>
#include <stdio.h>
#include <string.h>
#if HAVE_UNAME
#  include <sys/utsname.h>
#endif

#include <debug.hh>
#include <download.hh>
#include <glibwww.hh>
#include <libwww.hh>
#include <log.hh>
#include <string-utf.hh>

namespace {
  Logger debug("download");
  Logger libwwwDebug("libwww");
}
//______________________________________________________________________

string Download::userAgent;

namespace {

  extern "C"
  int tracer(const char* fmt, va_list args) {
    vfprintf(stderr, fmt, args);
    return HT_OK;
  }

  BOOL nonono(HTRequest*, HTAlertOpcode, int, const char*, void*,
              HTAlertPar*) {
    return NO;
  }
}

// Initialize (g)libwww
void Download::init() {
  HTAlertInit();
  if (libwwwDebug) {
    HTSetTraceMessageMask("flbtspuhox");
    HTTrace_setCallback(tracer);
  }

  HTEventInit(); // Necessary on Windows to initialize WinSock
  HTNet_setMaxSocket(32);

  /* These calls are necessary for redirections to work. (Why? Don't
     ask why - this is libwww, after all...) */
  HTList* converters = HTList_new();
  HTConverterInit(converters); // Register the default set of converters
  HTFormat_setConversion(converters); // Global converters for all requests

  HTAlert_setInteractive(YES);
  // HTPrint_setCallback(printer);
  glibwww_init("jigdo", JIGDO_VERSION);

  HTAlert_add(Download::alertCallback, HT_A_PROGRESS); // Progress reports
  HTAlert_add(nonono, static_cast<HTAlertOpcode>(
              HT_A_CONFIRM | HT_A_PROMPT | HT_A_SECRET | HT_A_USER_PW));
  // To get notified of errors, redirects etc.
  HTNet_addAfter(Download::afterFilter, NULL /*template*/, 0 /*param*/,
                 HT_ALL, HT_FILTER_MIDDLE);

  HTFTP_setTransferMode(FTP_BINARY_TRANSFER_MODE);

  //HTHost_setActivateRequestCallback(Download::activateRequestCallback);

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
    userAgent += " libwww/";
    userAgent += HTLib_version();
  }
}
//______________________________________________________________________

namespace {

  inline Download* getDownload(HTRequest* request) {
    return static_cast<Download*>(HTRequest_context(request));
  }
  inline Download* getDownload(HTStream* stream) {
    return reinterpret_cast<Download*>(stream);
  }

}
//______________________________________________________________________

Download::Download(const string& uri, Output* o)
    : uriVal(uri), resumeOffsetVal(0), resumeChecked(true), currentSize(0),
      outputVal(o), request(0), state(CREATED) {
# if DEBUG
  insideNewData = false;
# endif
  static const HTStreamClass downloadWriter = {
    "jigdoDownloadWriter", flush, free, abort, putChar, putString, write
  };
  vptr = &downloadWriter;

  /* The code below (e.g. in putChar()) silently assumes that the
     first data member's address of a Download object is identical to
     the object's address. The C++ standard makes no guarantee about
     this. :-/ */
  Assert(static_cast<void*>(this) == static_cast<void*>(&vptr));
  request = HTRequest_new();

  // Store within the HTRequest object a ptr to the corresponding Download
  HTRequest_setContext(request, static_cast<void*>(this));

  HTStream* writer = reinterpret_cast<HTStream*>(this); // Shudder... :-)
  HTRequest_setOutputFormat(request, WWW_SOURCE); // Raw data, no headers...
  HTRequest_setOutputStream(request, writer); // is sent to writer
  //HTRequest_setDebugStream(request, NULL); // body different from 200 OK
  HTRequest_setAnchor(request, HTAnchor_findAddress(uriVal.c_str()));

  // Remove libwww's User-Agent field and add our own
  HTRequest_setRqHd(request,
      static_cast<HTRqHd>(HTRequest_rqHd(request) & ~HT_C_USER_AGENT));
  HTRequest_addExtraHeader(request, "User-Agent",
                           const_cast<char*>(userAgent.c_str()));
}
//________________________________________

Download::~Download() {
  //outputVal->download_deleted();
  //if (destroyRequestId != 0) g_source_remove(destroyRequestId);
  if (request != 0) HTRequest_delete(request);
}
//______________________________________________________________________

/* Important: Our HTRequest object can be used several times - we must ensure
   that any non-default settings (e.g. "Range" header) are reset before
   reusing it. */
void Download::run(uint64 resumeOffset, bool pragmaNoCache) {
  debug("run resumeOffset=%1", resumeOffset);
  Assert(outputVal != 0); // Must have set up output
  Paranoid(request != 0); // Don't call this after stop()
  //Assert(destroyRequestId == 0); // No pending callback allowed from now on
  state = RUNNING;
  resumeOffsetVal = currentSize = resumeOffset;

  // Force reload from originating server, bypassing proxies?
  if (pragmaNoCache)
    HTRequest_addGnHd(request, HT_G_PRAGMA_NO_CACHE);
  else
    HTRequest_setGnHd(request, static_cast<HTGnHd>(HTRequest_gnHd(request)
                                                   & ~HT_G_PRAGMA_NO_CACHE));

  // Shall we resume the download from a certain offset?
  HTRequest_deleteRange(request); // Delete old range, if any
  if (resumeOffset > 0) {
    /* TODO: If we contacted the host earlier, we could use
       HTHost_isRangeUnitAcceptable() to check whether the host accepts range
       requests. */

    // range can be "345-999" (both inclusive) or "345-"; offsets start at 0
    string range;
    append(range, resumeOffset);
    range += '-';
    HTRequest_addRange(request, "bytes", const_cast<char*>(range.c_str()));
    /* A server can ignore the range for various reasons (unsupported,
       requested offset outside file) - this can be detected by the
       presence/absence of a content-range header in its answer (header
       present and "206 Partial Response" <=> partial retrieval OK). Check
       later whether Content-Range is present and correct. */
    resumeChecked = false;
  }

  if (HTLoad(request, NO) == NO) generateError();
  return;
}
//______________________________________________________________________

/* Implementation for the libwww HTStream functionality - forwards the
   calls to the Output object.
   Return codes: HT_WOULD_BLOCK, HT_ERROR, HT_OK, >0 to pass back. */

int Download::flush(HTStream* me) {
  if (debug) debug("flush %1", getDownload(me));
  return HT_OK;
}
int Download::free(HTStream* me) {
  if (debug) debug("free %1", getDownload(me));
  return HT_OK;
}
int Download::abort(HTStream* me, HTList*) {
  if (debug) debug("abort %1", getDownload(me));
  return HT_OK;
}
//________________________________________

#if DEBUG
#  define NEWDATA_BEGIN self->insideNewData = true;
#  define NEWDATA_END   self->insideNewData = false;
#else
#  define NEWDATA_BEGIN
#  define NEWDATA_END
#endif

int Download::putChar(HTStream* me, char c) {
  Download* self = getDownload(me);
  NEWDATA_BEGIN;
  //if (self->destroyRequestId != 0) return HT_OK;
  if (!self->resumeChecked && self->resumeCheck()) return HT_ERROR;
  //if (self->activated == 2) { self->state = ERROR; return HT_ERROR; }
  if (self->state == PAUSE_SCHEDULED) self->pauseNow();
  self->currentSize += 1;
  self->outputVal->download_data(reinterpret_cast<const byte*>(&c),
                                 1, self->currentSize);
  NEWDATA_END;
  return HT_OK;
}
int Download::putString(HTStream* me, const char* s) {
  Download* self = getDownload(me);
  NEWDATA_BEGIN;
  //if (self->destroyRequestId != 0) return HT_OK;
  if (!self->resumeChecked && self->resumeCheck()) return HT_ERROR;
  //if (self->activated == 2) { self->state = ERROR; return HT_ERROR; }
  if (self->state == PAUSE_SCHEDULED) self->pauseNow();
  size_t len = strlen(s);
  self->currentSize += len;
  self->outputVal->download_data(reinterpret_cast<const byte*>(s),
                                 len, self->currentSize);
  NEWDATA_END;
  return HT_OK;
}
int Download::write(HTStream* me, const char* s, int l) {
  Download* self = getDownload(me);
  NEWDATA_BEGIN;
  //if (self->destroyRequestId != 0) return HT_OK;
  if (!self->resumeChecked && self->resumeCheck()) return HT_ERROR;
  //if (self->activated == 2) { self->state = ERROR; return HT_ERROR; }
  if (self->state == PAUSE_SCHEDULED) self->pauseNow();
  size_t len = static_cast<size_t>(l);
  self->currentSize += len;
  self->outputVal->download_data(reinterpret_cast<const byte*>(s),
                                 len, self->currentSize);
  NEWDATA_END;
  return HT_OK;
}
//______________________________________________________________________

bool Download::resumeCheck() {
  resumeChecked = true;

  HTNet* net = HTRequest_net(request);
  unsigned protocol = HTProtocol_id(HTNet_protocol(net));
  if (protocol != 80 && protocol != 21) return false;
  // The check below only works for HTTP (and FTP in hacked libwww 5.4.0)

  do { // Never loops, just to break out
    HTAssocList* ranges = HTResponse_range(HTRequest_response(request));
    if (ranges == 0) break;
    HTAssoc* r = static_cast<HTAssoc*>(HTAssocList_nextObject(ranges));
    if (r == 0) break;
    if (strcmp(HTAssoc_name(r), "bytes") != 0) break;
    const char* s = HTAssoc_value(r);
    if (s == 0) break;
    uint64 startOff = 0;
    while (*s >= '0' && *s <= '9') startOff = startOff * 10 + (*s++ - '0');
    debug("resumeCheck: resumeOffsetVal=%1, server offset=%2",
          resumeOffsetVal, startOff);
    if (startOff == resumeOffsetVal)
      return false;
  } while (false);

  // Error, resume not possible (e.g. because it's a HTTP 1.0 server)
  debug("resumeCheck: Resume not supported");
  state = ERROR;
  string error = _("Resume not supported by server");
  outputVal->download_failed(&error);
  return true;
}
//______________________________________________________________________

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

  if (debug && op != HT_PROG_READ)
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
//   case HT_PROG_WRITE:
//     self->outputVal->info(_("Sending request"));
//     break;
//   case HT_PROG_DONE:
//     self->output->finish(); is done by afterFilter instead
//     break;
//   case HT_PROG_INTERRUPT:
//     self->outputVal->info(_("Interrupted"));
//     break;
//   case HT_PROG_TIMEOUT:
//     self->outputVal->info(_("Timeout"));
//     break;
  default:
    break;
  }

  return YES; // Value only relevant for op == HT_A_CONFIRM
}
//______________________________________________________________________

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
  case HT_ERROR: msg = "HT_ERROR"; break;
  case HT_LOADED: msg = "HT_LOADED"; break;
  case HT_PARTIAL_CONTENT: msg = "HT_PARTIAL_CONTENT"; break;
  case HT_NO_DATA: msg = "HT_NO_DATA"; break;
  case HT_NO_ACCESS: msg = "HT_NO_ACCESS"; break;
  case HT_NO_PROXY_ACCESS: msg = "HT_NO_PROXY_ACCESS"; break;
  case HT_RETRY: msg = "HT_RETRY"; break;
  case HT_PERM_REDIRECT: msg = "HT_PERM_REDIRECT"; break;
  case HT_TEMP_REDIRECT: msg = "HT_TEMP_REDIRECT"; break;
  }
  debug("Status %1 (%2) for %3 obj %4", status, msg, self->uri(), self);
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
  if (status >= 0 || status == HT_INTERRUPTED || status == HT_TIMEOUT) {
    self->generateError(INTERRUPTED);
    return HT_OK;
  }

  self->generateError();
  return HT_OK;
}
//______________________________________________________________________

/* This is dirty, dirty - don't look... Unfortunately, the socket used for
   FTP data connections isn't publically accessible. */

extern "C" {

  // Taken from libwww, HTFTP.c
  typedef enum _HTFTPState {
    FTP_SUCCESS = -2,
    FTP_ERROR = -1,
    FTP_BEGIN = 0,
    FTP_NEED_CCON,                                   /* Control connection */
    FTP_NEED_LOGIN,
    FTP_NEED_DCON,                                      /* Data connection */
    FTP_NEED_DATA,
    FTP_NEED_SERVER                              /* For directory listings */
  } HTFTPState;

  // Taken from libwww, HTFTP.c
  typedef struct _ftp_ctrl {
    HTChunk *           cmd;
    int                 repcode;
    char *              reply;
    char *              uid;
    char *              passwd;
    char *              account;
    HTFTPState          state;                  /* State of the connection */
    int                 substate;               /* For hierarchical states */
    BOOL                sent;                     /* Read or write command */
    BOOL                cwd;                                   /* Done cwd */
    BOOL                reset;                          /* Expect greeting */
    FTPServerType       server;                          /* Type of server */
    HTNet *             cnet;                        /* Control connection */
    HTNet *             dnet;                           /* Data connection */
    // This is the HTNet^^^^ that we need to access to find the socket! -- RA
    BOOL                alreadyLoggedIn;
  } ftp_ctrl;

}

/* Pause download by removing the request's socket from the list of sockets
   to call select() with. */
void Download::pauseNow() {
  Paranoid(state == PAUSE_SCHEDULED);
  state = PAUSED;
  if (request == 0) return;

  /* The HTNet object whose socket we'll unregister from the event loop. This
     will prevent more data from being delivered to it, effectively pausing
     the request. */
  HTNet* net = HTRequest_net(request);

  unsigned protocol = HTProtocol_id(HTNet_protocol(net));
  if (protocol == 21) {
    /* Protocol is FTP, which uses a control connection (which corresponds to
       the main HTNet object) and a data connection. We need the HTNet object
       for the latter. */
    ftp_ctrl* ctrl = static_cast<ftp_ctrl*>(HTNet_context(net));
    net = ctrl->dnet;
  }

#if 1
  HTChannel* channel = HTHost_channel(HTNet_host(net));
  HTEvent_unregister(HTChannel_socket(channel), HTEvent_READ);
#else
  // Unregister socket
  HTEvent_setTimeout(HTNet_event(net), -1); // No timeout for the socket
  SOCKET socket = HTNet_socket(net);
  HTEvent_unregister(socket, HTEvent_READ);
  debug("pauseNow: unregistered socket %1, event %2, cbf %3",
        int(socket), (void*)HTNet_event(net), (void*)HTNet_event(net)->cbf);
#endif
}

// Analogous to pauseNow() above
void Download::cont() {
  if (state == PAUSE_SCHEDULED) state = RUNNING;
  if (state == RUNNING) return;
  Assert(paused());
  state = RUNNING;
  if (request == 0) return;

  HTNet* net = HTRequest_net(request);
  unsigned protocol = HTProtocol_id(HTNet_protocol(net));
  if (protocol == 21) {
    ftp_ctrl* ctrl = static_cast<ftp_ctrl*>(HTNet_context(net));
    net = ctrl->dnet;
  }

#if 1
  HTHost* host = HTNet_host(net);
  HTHost_unregister(host, net, HTEvent_READ);
  HTHost_register(host, net, HTEvent_READ);
#else
  // Register socket again
  /* For some weird reason the timeout gets reset to 0 somewhere, which
     causes *immediate* timeouts with glibwww - fix that. */
  HTEvent* event = HTNet_event(net);
  HTEvent_setTimeout(event, HTHost_eventTimeout());
  SOCKET socket = HTNet_socket(net);
  HTEvent_register(socket, HTEvent_READ, event);
  debug("cont: registered socket %1, event %2, cbf %3",
        int(socket), (void*)event, (void*)event->cbf);
#endif
}
//______________________________________________________________________

void Download::stop() {
# if DEBUG
  /* stop() must not be called when libwww is delivering data, i.e. you must
     not call it from anything which is called by your download_data()
     method. */
  Assert(insideNewData == false);
# endif
  if (request != 0) {
#   if DEBUG
    int status = HTNet_killPipe(HTRequest_net(request));
    debug("stop: HTNet_killPipe() returned %1", status);
#   else
    HTNet_killPipe(HTRequest_net(request));
#   endif
  }
  state = SUCCEEDED;
  outputVal->download_succeeded();
}
//______________________________________________________________________

// Call output->error() with appropriate string taken from request object
/* If this is called, the Download is assumed to have failed in a
   non-recoverable way. */
void Download::generateError(State newState) {
  /* If state is ERROR or INTERRUPTED, we've already called download_failed()
     - don't do it again. Ditto for SUCCEEDED and download_succeeded() */
  if (state == ERROR || state == INTERRUPTED || state == SUCCEEDED) return;

  Assert(request != 0);
  HTList* errList = HTRequest_error(request);
  HTError* err;
  int errIndex = 0;
  while ((err = static_cast<HTError*>(HTList_removeFirstObject(errList)))) {
    errIndex = HTError_index(err);
    debug("  %1 %2",
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

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

  Return type for functions to show success or failure

*/

#ifndef STATUS_HH
#define STATUS_HH

/** Simple success/failure return type */
class Status {
public:
  static const bool OK = false;
  static const bool FAILED = true;
  explicit Status(bool c) : code(c) { }
  Status(const Status& x) : code(x.code) { }
  Status& operator=(const Status& x) { code = x.code; return *this; }
  bool ok() const { return code == OK; }
  bool failed() const { return code == FAILED; }
  /* Default dtor */
  /* Intentionally no operator bool() - should write ok() or failed()
     explicitly in if() conditions! */
  bool code;
private:
  // Prevent implicit conversions to bool
  explicit Status(int);
  explicit Status(unsigned);
  explicit Status(void*);
};

static const Status OK = Status(Status::OK);
static const Status FAILED = Status(Status::FAILED);
//______________________________________________________________________

/** Version of Status which can contain more than 2 values. Explicitly making
    this a separate class so users (hopefully) notice that there are other
    states beyond succeeded/failed. */
class XStatus {
public:
//   static const int OK = 0;
//   static const int FAILED = -1;
  XStatus(const Status& x) : code(x.ok() ? 0 : -1) { }
  explicit XStatus(int c) : code(c) { }
  XStatus(const XStatus& x) : code(x.code) { }
  XStatus& operator=(const XStatus& x) { code = x.code; return *this; }
  XStatus& operator=(const Status& x) {
    code = (x.ok() ? 0 : -1);
    return *this;
  }
  /** Not calling these ok()/failed() to remind you that this is a
      XStatus */
  bool xok() const { return code == 0; }
  bool xfailed() const { return code == -1; }
  //bool other() const { return code != 0 && code != -1; }
  bool returned(int x) const { return code == x; }
  /* Default dtor */
  /* Intentionally no operator bool() - should write ok() or failed()
     explicitly in if() conditions! */
  int code;
};

#endif

/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  A variant of auto_ptr for pointers to arrays

*/

#ifndef AUTOPTR_HH
#define AUTOPTR_HH

/** A variant of std::auto_ptr for pointers to arrays. Aside note:
    Does not implement the "const auto_ptr copy protection". */
template <class X> class ArrayAutoPtr {
public:
  typedef X element_type;

  explicit ArrayAutoPtr(X* p = 0) throw() : ptr(p) { }
  ArrayAutoPtr(ArrayAutoPtr& a) throw() : ptr(a.release()) { }
  template <class Y> ArrayAutoPtr(ArrayAutoPtr<Y>& a) throw()
    : ptr(a.release()) { }
  ArrayAutoPtr& operator=(ArrayAutoPtr& a) throw() {
    if (&a != this) { delete[] ptr; ptr = a.release(); }
    return *this;
  }
  template <class Y>
  ArrayAutoPtr& operator=(ArrayAutoPtr<Y>& a) throw() {
    if (a.get() != this->get()) { delete[] ptr; ptr = a.release(); }
    return *this;
  }
  ~ArrayAutoPtr() throw() { delete[] ptr; }

  X& operator*() const throw() { return *ptr; }
  X* operator->() const throw() { return ptr; }
  X* get() const throw() { return ptr; }
  X* release() throw() { X* tmp = ptr; ptr = 0; return tmp; }
  void reset(X* p = 0) throw() { delete[] ptr; ptr = p; }
private:
  X* ptr;
};

#endif

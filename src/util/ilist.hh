/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

  Intrusive list, i.e. every list member needs to derive from IListBase

*/

#ifndef ILIST_HH
#define ILIST_HH

#include <config.h>

#include <debug.hh>
#include <log.hh>
//______________________________________________________________________

/** Never use this type or its members in your code apart from deriving
    publicly from it. */
struct IListBase {
  IListBase() : iListBase_prev(0), iListBase_next(0) {
    //msg("IListBase %1", this);
  }
  ~IListBase() {
    //msg("~IListBase %1", this);
    iListBase_remove();
  }
  /** May use this to unlink this object from its list, if any */
  void iListBase_remove() {
    if (iListBase_prev == 0) return;
    iListBase_prev->iListBase_next = iListBase_next;
    iListBase_next->iListBase_prev = iListBase_prev;
    iListBase_prev = iListBase_next = 0;
  }
/*private:*/
  IListBase* iListBase_prev;
  IListBase* iListBase_next;
};

template <class T>
class IList {
public:

  typedef unsigned size_type;
  typedef T value_type;
  class iterator;
  //class const_iterator;
  friend class iterator;
  //friend class const_iterator;
  typedef T& reference;
  //typedef const T& const_reference;

  IList() { e.iListBase_prev = e.iListBase_next = &e; }
  bool empty() const { return e.iListBase_next == &e; }
  void push_back(T& x) {
    //msg("IList::push_back %1", &x);
    Assert(x.iListBase_prev == 0 && x.iListBase_next == 0);
    x.iListBase_prev = e.iListBase_prev;
    x.iListBase_next = &e;
    x.iListBase_prev->iListBase_next = &x;
    x.iListBase_next->iListBase_prev = &x;
  }

  T& front() const { return *static_cast<T*>(e.iListBase_next); }
  T& back() const { return *static_cast<T*>(e.iListBase_prev); }

  inline iterator begin() const { return iterator(e.iListBase_next); }
  inline iterator end() const { return iterator(const_cast<IListBase*>(&e));}

private:
  IListBase e;
};

template <class T>
class IList<T>::iterator {
public:
  iterator(IListBase* pp) : p(pp) { }
  T& operator*() { return *getT(); }
  const T& operator*() const { return *getT(); }
  T* operator->() { return getT(); }
  const T* operator->() const { return getT(); }
  iterator& operator++() { p = p->iListBase_next; return *this; }
  iterator& operator--() { p = p->iListBase_prev; return *this; }
  bool operator==(const iterator i) const { return p == i.p; }
  bool operator!=(const iterator i) const { return p != i.p; }
private:
  T* getT() { return static_cast<T*>(p); }
  IListBase* p;
};

// template <class T>
// class IList::iterator {
// public:
// private:
//   IListBase* p;
// };

#endif

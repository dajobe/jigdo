/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

*//** @file

  Intrusive list, ie every list member needs to derive publically from
  IListBase

  A speciality is that list members will remove themselves from their list
  automatically from their dtor.

*/

#ifndef ILIST_HH
#define ILIST_HH

#include <config.h>

#include <debug.hh>
#include <log.hh>
//______________________________________________________________________

/** Derived classes can be list members. */
class IListBase {
  template<class T> friend class IList;
public:
  IListBase() : iListBase_prev(0), iListBase_next(0) {
    //msg("IListBase %1", this);
  }
  ~IListBase() {
    //msg("~IListBase %1", this);
    iList_remove();
  }
  /** May use this to unlink this object from its list, if any */
  void iList_remove() {
    if (iListBase_prev == 0) return;
    Paranoid(iListBase_next != 0);
    iListBase_prev->iListBase_next = iListBase_next;
    iListBase_next->iListBase_prev = iListBase_prev;
    iListBase_prev = iListBase_next = 0;
  }

private:
  IListBase* iListBase_prev;
  IListBase* iListBase_next;
};

/** The list object. */
template<class T>
class IList {
public:
  typedef unsigned size_type;
  typedef T value_type;
  class iterator;
  class const_iterator;
  friend class iterator;
  friend class const_iterator;
  typedef T& reference;
  typedef const T& const_reference;

  IList() { e.iListBase_prev = e.iListBase_next = &e; }
  /** Releases all member objects from the list, does not delete them. */
  ~IList() {
    IListBase* p = e.iListBase_next;
    while (p != &e) {
      IListBase* q = p->iListBase_next;
      p->iListBase_prev = p->iListBase_next = 0;
      p = q;
    }
    e.iListBase_prev = e.iListBase_next = 0;
  }
  bool empty() const { return e.iListBase_next == &e; }
  void push_back(T& x) {
    //msg("IList::push_back %1", &x);

    // Object must not already be a list member
    Assert(x.iListBase_prev == 0 && x.iListBase_next == 0);

    x.iListBase_prev = e.iListBase_prev;
    x.iListBase_next = &e;
    x.iListBase_prev->iListBase_next = &x;
    x.iListBase_next->iListBase_prev = &x;
  }
  void push_front(T& x) {
    // Object must not already be a list member
    Assert(x.iListBase_prev == 0 && x.iListBase_next == 0);

    x.iListBase_prev = &e;
    x.iListBase_next = e.iListBase_next;
    x.iListBase_prev->iListBase_next = &x;
    x.iListBase_next->iListBase_prev = &x;
  }

  T& front() const { return *static_cast<T*>(e.iListBase_next); }
  T& back() const { return *static_cast<T*>(e.iListBase_prev); }

  inline iterator begin() {
    return iterator(e.iListBase_next); }
  inline iterator end() {
    return iterator(&e); }
  inline const_iterator begin() const {
    return const_iterator(e.iListBase_next); }
  inline const_iterator end() const {
    return const_iterator(&e); }

private:
  // For the iterator class, which cannot be a friend of IListBase
  static inline IListBase* next(const IListBase* ilb) {
    return ilb->iListBase_next; }
  static inline IListBase* prev(const IListBase* ilb) {
    return ilb->iListBase_prev; }

  IListBase e;
};

/** iterator for an IList object */
template<class T>
class IList<T>::iterator {
  friend class const_iterator;
public:
  iterator(IListBase* pp) : p(pp) { }
  iterator(const iterator& i) : p(i.p) { }
  iterator& operator=(const iterator& i) { p = i.p; return *this; }
  iterator& operator=(const const_iterator& i) { p = i.p; return *this; }

  T& operator*() { return *getT(); }
  const T& operator*() const { return *getT(); }
  T* operator->() { return getT(); }
  const T* operator->() const { return getT(); }
  iterator& operator++() { p = IList<T>::next(p); return *this; }
  iterator& operator--() { p = IList<T>::prev(p); return *this; }
  bool operator==(const iterator i) const { return p == i.p; }
  bool operator!=(const iterator i) const { return p != i.p; }
private:
  //T* getT() { return reinterpret_cast<T*>(p); }
  // Will not work if IListBase is an inaccessible base of T:
  T* getT() { return static_cast<T*>(p); }
  IListBase* p;
};

/** const_iterator for an IList object */
template<class T>
class IList<T>::const_iterator {
  friend class iterator;
public:
  const_iterator(IListBase* pp) : p(pp) { }
  const_iterator(const IListBase* pp) : p(pp) { }
  explicit const_iterator(const iterator& i) : p(i.p) { }
  const_iterator& operator=(const iterator& i) { p = i.p; return *this; }
  const_iterator(const const_iterator& i) : p(i.p) { }
  const_iterator& operator=(const const_iterator& i) { p = i.p; return *this; }

  const T& operator*() const { return *getT(); }
  const T* operator->() const { return getT(); }
  const_iterator& operator++() { p = IList<T>::next(p); return *this; }
  const_iterator& operator--() { p = IList<T>::prev(p); return *this; }
  bool operator==(const const_iterator i) const { return p == i.p; }
  bool operator!=(const const_iterator i) const { return p != i.p; }
private:
  //const T* getT() const { return reinterpret_cast<const T*>(p); }
  // Will not work if IListBase is an inaccessible base of T:
  const T* getT() const { return static_cast<const T*>(p); }
  const IListBase* p;
};

#endif

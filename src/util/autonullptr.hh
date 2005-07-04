/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

*//** @file

  A pointer which gets set to null if the pointed-to object is
  deleted. Somewhat equivalent to "weak references" in Java.

  <pre>
  class MyClass : public AutoNullPtrBase<MyClass> {
    ... your class members here ...
  } myClass;
  AutoNullPtr<MyClass> ptr = &amp;myClass;</pre>

  Implementation: AutoNullPtrBase contains the head of a linked list of
  AutoNullPtrs, sets them all to 0 from its dtor. ~AutoNullPtr removes itself
  from that list.

*/

#ifndef AUTONULLPTR_HH
#define AUTONULLPTR_HH

#include <ilist.hh>
//______________________________________________________________________

template<class T> class AutoNullPtrBase;

/** A pointer which gets set to null if the pointed-to object is deleted */
template<class T>
class AutoNullPtr : public IListBase {
public:
  AutoNullPtr() : IListBase(), p(0) { }
  AutoNullPtr(T* ptr) : IListBase(), p(ptr) { addSelf(); }
  AutoNullPtr(const AutoNullPtr<T>& b) : IListBase(), p(b.get()) { addSelf(); }
  ~AutoNullPtr() { iList_remove(); }

  AutoNullPtr<T>& operator=(const AutoNullPtr<T>& b) {
    iList_remove(); p = b.get(); addSelf(); return *this;
  }
  AutoNullPtr<T>& operator=(T* ptr) {
    iList_remove(); p = ptr; addSelf(); return *this;
  }

  T* get()        const { return p; }
  T& operator*()  const { return *p; }
  T* operator->() const { return p; }
  operator bool() const { return p != 0; }

private:
  void addSelf() { if (p != 0) p->list.push_back(*this); }
  T* p;
};
//______________________________________________________________________

/** Derive from this class to make your class instances referenceable by
    AutoNullPtr */
template<class T>
class AutoNullPtrBase {
public:
  AutoNullPtrBase() { }
  ~AutoNullPtrBase() { while (!list.empty()) list.front() = 0; }
private:
  friend class AutoNullPtr<T>;
  IList<AutoNullPtr<T> > list;
};
//______________________________________________________________________

/** @name
    The regular pointer tests */
//@{
template<class T>
inline bool operator==(const AutoNullPtr<T>& a, const T* b) {
  return a.get() == b;
}
template<class T>
inline bool operator==(const T* b, const AutoNullPtr<T>& a) {
  return a.get() == b;
}
template<class T>
inline bool operator==(const AutoNullPtr<T>& a, const AutoNullPtr<T>& b) {
  return a.get() == b.get();
}

template<class T>
inline bool operator!=(const AutoNullPtr<T>& a, const T* b) {
  return a.get() != b;
}
template<class T>
inline bool operator!=(const T* b, const AutoNullPtr<T>& a) {
  return a.get() != b;
}
template<class T>
inline bool operator!=(const AutoNullPtr<T>& a, const AutoNullPtr<T>& b) {
  return a.get() != b.get();
}

template<class T>
inline bool operator<(const AutoNullPtr<T>& a, const T* b) {
  return a.get() < b;
}
template<class T>
inline bool operator<(const T* b, const AutoNullPtr<T>& a) {
  return a.get() < b;
}
template<class T>
inline bool operator<(const AutoNullPtr<T>& a, const AutoNullPtr<T>& b) {
  return a.get() < b.get();
}

template<class T>
inline bool operator>(const AutoNullPtr<T>& a, const T* b) {
  return a.get() > b;
}
template<class T>
inline bool operator>(const T* b, const AutoNullPtr<T>& a) {
  return a.get() > b;
}
template<class T>
inline bool operator>(const AutoNullPtr<T>& a, const AutoNullPtr<T>& b) {
  return a.get() > b.get();
}

template<class T>
inline bool operator<=(const AutoNullPtr<T>& a, const T* b) {
  return a.get() <= b;
}
template<class T>
inline bool operator<=(const T* b, const AutoNullPtr<T>& a) {
  return a.get() <= b;
}
template<class T>
inline bool operator<=(const AutoNullPtr<T>& a, const AutoNullPtr<T>& b) {
  return a.get() <= b.get();
}

template<class T>
inline bool operator>=(const AutoNullPtr<T>& a, const T* b) {
  return a.get() >= b;
}
template<class T>
inline bool operator>=(const T* b, const AutoNullPtr<T>& a) {
  return a.get() >= b;
}
template<class T>
inline bool operator>=(const AutoNullPtr<T>& a, const AutoNullPtr<T>& b) {
  return a.get() >= b.get();
}
//@}
//______________________________________________________________________

#endif

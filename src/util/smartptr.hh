/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2 or
  later. See the file COPYING for details.

  Smart pointer, i.e. pointer-like object that maintains a count for
  how many times an object is referenced, and only deletes the object
  when the last smart pointer to it is destroyed.

  To make it possible to create smart pointers to objects of a class,
  the class must inherit from SmartPtrBase.

  NB: SmartPtrBase must only be inherited from *once*, so derive
  virtually if it appears multiple times:

  class MyClass : public virtual SmartPtrBase {
    ...
  };

*/

#ifndef SMARTPTR_HH
#define SMARTPTR_HH

#ifdef DEBUG
#  include <cstdlib>
#  include <iostream>
#endif
//______________________________________________________________________

struct SmartPtr_lockStatic;

/* The version of SmartPtrBase below needs the following:
     class Base    : public SmartPtrBase { ... };
     class Derived : public Base { ... };
   in order to allow a Derived object's address to be assigned to a
   SmartPtr<Base>. Its only fault is that its member must be public. */
struct SmartPtrBase {
  //friend template class<X> SmartPtr<X>;
  friend struct SmartPtr_lockStatic;
  SmartPtrBase() /*throw()*/ : smartPtr_refCount(0) { }
  int smartPtr_refCount;
};

/* The version of SmartPtrBase below needs the following:
   class Base    : virtual public SmartPtrBase<Base> { ... };
   class Derived : virtual public SmartPtrBase<Derived>, public Base { ... };
   in order to allow a Derived object's address to be assigned to a
   SmartPtr<Base>. */
//  template<class X> class SmartPtr;
//
//  template<class X>
//  class SmartPtrBase {
//    friend class SmartPtr<X>;
//  public:
//    SmartPtrBase() /*throw()*/ : smartPtr_refCount(0) { }
//  private:
//    int smartPtr_refCount;
//  };
//______________________________________________________________________

/* If static objects are accessed through smart pointers, ensure that
   there are no attempts to delete them, by defining a non-static
   SmartPtr_lockStatic(object), which MUST be DEFINED (not declared)
   AFTER the object being locked, in the SAME translation
   unit. Otherwise, order of construction is not defined. */
struct SmartPtr_lockStatic {
  SmartPtr_lockStatic(SmartPtrBase& obj) { ++obj.smartPtr_refCount; }
  ~SmartPtr_lockStatic() { }
};
//______________________________________________________________________

// There are no implicit conversions from/to the actual pointer.
template<class X>
class SmartPtr {
public:
  typedef X element_type;

  SmartPtr() : ptr(0) {
#   ifdef DEBUG_SMARTPTR
    cerr << this << " SmartPtr()" << endl;
#   endif
  }
  ~SmartPtr() {
#   ifdef DEBUG_SMARTPTR
    cerr << this << " ~SmartPtr() " << ptr << "/"
         << (ptr == 0 ? 0 : ptr->smartPtr_refCount) << endl;
#   endif
    decRef();
  }

  // init from SmartPtr<X>
  SmartPtr(const SmartPtr& x) : ptr(x.get()) {
#   ifdef DEBUG_SMARTPTR
    cerr << this << " SmartPtr(SP " << x.get() << ")" << endl;
#   endif
    incRef();
  }
  // init from SmartPtr to other type; only works if implicit conv. possible
  template<class Y> SmartPtr(const SmartPtr<Y>& y) : ptr(y.get()) {
#   ifdef DEBUG_SMARTPTR
    cerr << this << " SmartPtr(SP<Y> " << y.get() << ")" << endl;
#   endif
    incRef();
  }
  // init from pointer
  explicit SmartPtr(X* x) : ptr(x) {
#   ifdef DEBUG_SMARTPTR
    cerr << this << " SmartPtr(" << x << ")" << endl;
#   endif
    incRef();
  }

  /* This one is necessary, the compiler will *not* generate one from
     the template below. */
  SmartPtr& operator=(const SmartPtr& x) {
#   ifdef DEBUG_SMARTPTR
    cerr << this << " SmartPtr " << ptr << " = SP<X> " << x.get() << endl;
#   endif
    if (ptr != x.get()) { decRef(); ptr = x.get(); incRef(); }
    return *this;
  }
  template<class Y> SmartPtr& operator=(const SmartPtr<Y>& y) {
#   ifdef DEBUG_SMARTPTR
    cerr << this << " SmartPtr " << ptr << " = SP<Y> " << y.get() << endl;
#   endif
    if (ptr != y.get()) { decRef(); ptr = y.get(); incRef(); }
    return *this;
  }
  template<class Y> SmartPtr& operator=(Y* y) {
#   ifdef DEBUG_SMARTPTR
    cerr << this << " SmartPtr " << ptr << " = " << y << endl;
#   endif
    if (ptr != y) { decRef(); ptr = y; incRef(); }
    return *this;
  }

  X& operator*()  const { return *ptr; }
  X* operator->() const { return ptr; }
  X* get()        const { return ptr; }
  X* release() { // relinquish ownership, but never delete
#   ifdef DEBUG
    if (ptr != 0 && ptr->SmartPtrBase::smartPtr_refCount == 0)
      abort();
#   endif
    if (ptr != 0) --(ptr->SmartPtrBase::smartPtr_refCount);
    X* tmp = ptr; ptr = 0; return tmp;
  }
  void swap(SmartPtr& x) { X* tmp = ptr; ptr = x.ptr; x.ptr = tmp; }
  void clear() { decRef(); ptr = 0; }
  bool isNull() const { return ptr == 0; }

private:
  void incRef() {
    if (ptr != 0) ++(ptr->SmartPtrBase::smartPtr_refCount);
  }
  void decRef() {
#   ifdef DEBUG
    if (ptr != 0 && ptr->SmartPtrBase::smartPtr_refCount == 0)
      abort();
#   endif
    if (ptr != 0 && --(ptr->SmartPtrBase::smartPtr_refCount) <= 0)
      delete ptr;
  }
  X* ptr;
};
//____________________

template<class X>
inline SmartPtr<X> makeSmartPtr(X* x) { return SmartPtr<X>(x); }

// only delete if count is zero
// 'deleteSmart(x);' is equivalent to '{ SmartPtr<X> tmp(x); }'
template<class X> // need template for 'delete ptr' to call the right dtor
inline bool deleteSmart(X* ptr) {
  if (ptr != 0 && ptr->SmartPtrBase::smartPtr_refCount <= 0) {
    delete ptr; return true;
  } else {
    return false;
  }
}

template<class X>
inline X* releaseSmart(X* ptr) {
  if (ptr != 0) --ptr->SmartPtrBase::smartPtr_refCount;
  return ptr;
}
//____________________

template<class X> inline void swap(SmartPtr<X>& a, SmartPtr<X>& b) {
  a.swap(b);
}
template<class X>
inline bool operator<(const SmartPtr<X> a, const SmartPtr<X> b) {
  return a.get() < b.get();
}
template<class X>
inline bool operator>(const SmartPtr<X> a, const SmartPtr<X> b) {
  return a.get() > b.get();
}
template<class X>
inline bool operator<=(const SmartPtr<X> a, const SmartPtr<X> b) {
  return a.get() <= b.get();
}
template<class X>
inline bool operator>=(const SmartPtr<X> a, const SmartPtr<X> b) {
  return a.get() >= b.get();
};

// allow comparison with pointers
template<class X>
inline bool operator==(const SmartPtr<X> a, const void* b) {
  return a.get() == b;
}
template<class X>
inline bool operator==(const void* a, const SmartPtr<X> b) {
  return a == b.get();
}
template<class X>
inline bool operator!=(const SmartPtr<X> a, const void* b) {
  return a.get() != b;
}
template<class X>
inline bool operator!=(const void* a, const SmartPtr<X> b) {
  return a != b.get();
}
#endif

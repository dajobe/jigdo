dnl Process this file with autoconf to produce a configure script.
dnl $Id$
AC_INIT(src/jigdo-file.cc)
AC_CONFIG_HEADER(src/config.h)
dnl ______________________________________________________________________

dnl Version number of our program.
read tmpa tmpb JIGDO_VERSION < "$srcdir/jigdo.spec"
AC_DEFINE_UNQUOTED(JIGDO_VERSION, "$JIGDO_VERSION")
AC_SUBST(JIGDO_VERSION)
dnl ______________________________________________________________________

dnl installDevel is given 2 arguments: Name of "devel" package on
dnl Debian-based systems, name on RPM-based systems
if test -f "/etc/debian_version"; then
    installDevel() {
    AC_MSG_RESULT([   * (Your system appears to be Debian-based; try])
    AC_MSG_RESULT([   * installing the \`$1-dev' package.)])
    }
elif test -x "/usr/bin/rpm" -o -x "/usr/local/bin/rpm"; then
    installDevel() {
    AC_MSG_RESULT([   * (Your system appears to be RPM-based; try])
    AC_MSG_RESULT([   * installing the package named \`$2-devel' or])
    AC_MSG_RESULT([   * similar.)])
    }
else
    installDevel() {
    AC_MSG_RESULT([   * (If the software on your system is managed by a])
    AC_MSG_RESULT([   * package manager like RPM, try installing the])
    AC_MSG_RESULT([   * package named \`$2-devel' or similar.)])
    }
fi
dnl ______________________________________________________________________

dnl Checks for programs.
AC_PROG_AWK
AC_LANG_C
AC_PROG_CC
dnl Add -Wno-unused-parameter to CFLAGS if supported
dnl ORIG_CFLAGS="$CFLAGS"
dnl CFLAGS="$CFLAGS -Wno-unused-parameter" # GCC >=3.0
dnl AC_TRY_COMPILE(, int x = 0, , CFLAGS="$ORIG_CFLAGS")
AC_LANG_CPLUSPLUS
AC_PROG_CXX
CFLAGS=`echo "$CFLAGS" | sed 's/\(^\| \)-g\( \|$\)/ /'`
CXXFLAGS=`echo "$CXXFLAGS" | sed 's/\(^\| \)-g\( \|$\)/ /'`
IF_GXX2="#"
IFNOT_GXX2=""
if test "$GXX" = yes; then
    AC_MSG_CHECKING(for GCC version)
    gccver=`$CXX -dumpversion`
    AC_MSG_RESULT($gccver);
    case "$gccver" in
        2.*)
            IF_GXX2=""
            IFNOT_GXX2="#";;
        3.0|3.0.*|3.1|3.1.*|3.3|3.3.*)
        #AC_MSG_RESULT([   * Big file support may be broken in GCC 3.2 and])
        #AC_MSG_RESULT([   * 3.3! If you compile jigdo on Linux (and maybe])
        #AC_MSG_RESULT([   * other OSes), it might not be able to create])
        #AC_MSG_RESULT([   * files bigger than 2 GB, such as DVD images.])
        #AC_MSG_RESULT([   * (Try GCC 2.95 or GCC 3.4 or later)])
    esac
fi
AC_SUBST(IF_GXX2)
AC_SUBST(IFNOT_GXX2)
AC_PROG_INSTALL
dnl ____________________

dnl Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_C_BIGENDIAN
AC_MSG_CHECKING(whether we are compiling for Windows)
AC_EGREP_CPP(poorsoul, [
#if defined WINNT || defined __WINNT || defined __WINNT__ || defined WIN32 \
    || defined _WIN32 || defined __WIN32 || defined __WIN32__
  poorsoul
#endif], is_windows=yes, is_windows=no)
AC_MSG_RESULT($is_windows)
if test "$is_windows" = yes; then
    AC_DEFINE(WINDOWS)
    exe=".exe"
    IF_WINDOWS=""
    IF_UNIX="#"
else
    AC_DEFINE(UNIX)
    exe=""
    IF_WINDOWS="#"
    IF_UNIX=""
fi
AC_SUBST(exe)
AC_SUBST(IF_WINDOWS)
AC_SUBST(IF_UNIX)
dnl ____________________

AC_CACHE_CHECK(whether the C++ compiler is recent enough,
               jigdo_cv_prog_cxx_recent,
    AC_TRY_COMPILE(
    #include <vector>
    namespace A {
      namespace B {
        template<typename T = double> struct X {
          template<class U> void function(T* t, U* u);
        };
        template<> struct X<void> { void function(void* t, int* u); };
      }
      void B::X<void>::function(void*, int*) { }
    },, jigdo_cv_prog_cxx_recent="yes", jigdo_cv_prog_cxx_recent="no")
)
if test "$jigdo_cv_prog_cxx_recent" = "no"; then
    AC_MSG_RESULT([   * Your compiler failed to recognize some advanced C++])
    AC_MSG_RESULT([   * constructs - it might be too old to compile jigdo.])
    AC_MSG_RESULT([   * In case compilation fails, try upgrading to a newer])
    AC_MSG_RESULT([   * compiler, e.g. GCC 2.95 or later.])
fi
AC_CACHE_SAVE
dnl ________________________________________

dnl C99-style vararg macros are not supported by GCC 2.95
AC_CACHE_CHECK([whether the C++ preprocessor supports variable-arg macros],
               jigdo_cv_prog_cxx_varmacro,
    AC_TRY_COMPILE(
    #if defined(__GNUC__) && __GNUC__ < 3
    #  define debug(_args...) do { } while (false)
    #else
    #  define debug(...) do { } while (false)
    #endif
    int foo() {
      int debug = 0; // No substitution!
      debug("!");
      debug("%1", debug);
      return debug;
    },, jigdo_cv_prog_cxx_varmacro="yes", jigdo_cv_prog_cxx_varmacro="no")
)
if test "$jigdo_cv_prog_cxx_varmacro" = "yes"; then
    AC_DEFINE(HAVE_VARMACRO, 1)
else
    AC_DEFINE(HAVE_VARMACRO, 0)
fi
dnl ________________________________________

AC_CHECK_PROG(have_wget, wget, yes, no)
if test "$have_wget" = "no"; then
    AC_MSG_RESULT([   * wget was not found on your system. Please install])
    AC_MSG_RESULT([   * it, it is needed by jigdo-lite.])
fi
dnl ________________________________________

AC_MSG_CHECKING(for value of --enable-debug)
AC_ARG_ENABLE(debug,
    [  --enable-debug          Compile in lots of additional debugging code],
    jigdo_debug=$enableval, jigdo_debug=no)
AC_MSG_RESULT(\"$jigdo_debug\")
if test "$jigdo_debug" = "yes"; then
    IF_DEBUG=""
    IFNOT_DEBUG="#"
    AC_DEFINE(DEBUG, 1)
    if test "$ac_cv_prog_cc_g" = "yes"; then CFLAGS="$CFLAGS -g"; fi
    if test "$ac_cv_prog_cxx_g" = "yes"; then CXXFLAGS="$CXXFLAGS -g"; fi
elif test "$jigdo_debug" != "no"; then
    AC_MSG_ERROR(Invalid argument to --enable-debug option)
else
    IF_DEBUG="#"
    IFNOT_DEBUG=""
    AC_DEFINE(DEBUG, 0)
fi
AC_SUBST(IF_DEBUG)
AC_SUBST(IFNOT_DEBUG)

dnl dnl cppunit
dnl CPPUNITCFLAGS='# `cppunit-config --cflags`'
dnl CPPUNITLIBS='# `cppunit-config --libs`'
dnl IF_CPPUNIT="#"
dnl AC_CHECK_PROG(have_cppunit_config, cppunit-config, yes, no)
dnl if test "$have_cppunit_config" = "yes"; then
dnl     CPPUNITCFLAGS="`cppunit-config --cflags`"
dnl     CPPUNITLIBS="`cppunit-config --libs`"
dnl     ORIG_LIBS="$LIBS"
dnl     LIBS="$LIBS $CPPUNITCFLAGS $CPPUNITLIBS"
dnl     AC_CACHE_CHECK([whether cppunit works],
dnl                    jigdo_cv_prog_cppunit,
dnl         AC_TRY_LINK(
dnl             [ #include <cppunit/extensions/TestFactoryRegistry.h>
dnl               #include <cppunit/ui/text/TestRunner.h>
dnl               #include <cppunit/CompilerOutputter.h>
dnl               using namespace CppUnit; ],
dnl             [ TextUi::TestRunner runner;
dnl               TestFactoryRegistry& registry =
dnl                   TestFactoryRegistry::getRegistry();
dnl               runner.addTest(registry.makeTest());
dnl               runner.run("", false); ],
dnl             jigdo_cv_prog_cppunit="yes", jigdo_cv_prog_cppunit="no"
dnl         )
dnl     )
dnl     LIBS="$ORIG_LIBS"
dnl     if test "$jigdo_cv_prog_cppunit" = "yes"; then
dnl         IF_CPPUNIT=""
dnl     else
dnl         AC_MSG_RESULT([   * cppunit does not work - disabled.])
dnl     fi
dnl elif test "$jigdo_debug" = "yes"; then
dnl     AC_MSG_RESULT([   * cppunit was not found on your system. Please])
dnl     AC_MSG_RESULT([   * install it if you want to run the unit tests.])
dnl fi
dnl AC_SUBST(IF_CPPUNIT)
dnl AC_SUBST(CPPUNITCFLAGS)
dnl AC_SUBST(CPPUNITLIBS)
dnl ______________________________________________________________________

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(stddef.h unistd.h limits.h string.h)

dnl Checks for libraries and accompanying header files

AC_CHECK_LIB(z, zlibVersion, have_zlib="-lz", have_zlib="no")
if test "$is_windows" = yes; then
    AC_CHECK_LIB(zdll, zlibVersion, have_zlib="-lzdll", )
fi
AC_CHECK_HEADER(zlib.h, have_zlib_h="yes", have_zlib_h="no")
if test "$have_zlib" = "no" -o "$have_zlib_h" = "no"; then
    AC_MSG_RESULT([   * Please install zlib, it is needed by all of the])
    AC_MSG_RESULT([   * programs.])
    installDevel "zlib1g" "zlib"
fi
if test "$have_zlib" != "no"; then LIBS="$have_zlib $LIBS"; fi


AC_CHECK_LIB(bz2, BZ2_bzCompressInit, have_bzlib="-lbz2", have_bzlib="no")
if test "$is_windows" = yes; then
    dnl For some reason, can't check for BZ2_bzCompressInit without #include <bzlib.h>
    AC_CHECK_LIB(bz2, main, have_bzlib="-lbz2", )
    AC_CHECK_LIB(bz2.dll, main, have_bzlib="-lbz2.dll", )
fi
AC_CHECK_HEADER(bzlib.h, have_bzlib_h="yes", have_bzlib_h="no")
if test "$have_bzlib" = "no" -o "$have_bzlib_h" = "no"; then
    AC_MSG_RESULT([   * Please install libbz2, it is needed by all of the])
    AC_MSG_RESULT([   * programs.])
    installDevel "libbz2" "libbz2"
fi
if test "$have_bzlib" != "no"; then LIBS="$have_bzlib $LIBS"; fi


AC_MSG_CHECKING(for value of --with-libdb)
AC_ARG_WITH(libdb,
    [  --without-libdb         Don't use libdb (it's necessary for jigdo-file's cache)], #'
    jigdo_libdb="$withval", jigdo_libdb="yes")
AC_MSG_RESULT(\"$jigdo_libdb\")
if test "$jigdo_libdb" = "yes"; then
    AC_CHECK_HEADER(db.h, have_db_h="yes", have_db_h="no")
    AC_MSG_CHECKING(for libdb version in db.h)
    printf "#include <db.h>\nDB_VERSION_MAJOR DB_VERSION_MINOR\n" >conftest.c
    set x `eval $ac_cpp conftest.c | egrep '^ *[[0-9]] *'`; v="$2"; vv="$3"
    AC_MSG_RESULT($v.$vv)
    if test "$v" -ne 3 -a "$v" -ne 4; then
        AC_MSG_RESULT([   * Warning: jigdo is only known to work with libdb])
        AC_MSG_RESULT([   * version 3 or 4 - maybe you should up/downgrade.])
        installDevel "libdb4.2" "libdb4"
    fi
    for db in "db-$v.$vv" "db$v.$vv" "db-$v" "db$v" "db"; do
#       Doesn't work: db.h does a "#define db_create db_create_4002"
#       AC_CHECK_LIB($db, db_create, have_libdb="$db", have_libdb="no")
        AC_MSG_CHECKING(for db_create in lib$db)
        oldLIBS="$LIBS"
        LIBS="$LIBS -l$db"
        AC_TRY_LINK([#include <db.h>], db_create(0, 0, 0),
                     have_libdb="yes", have_libdb="no")
        AC_MSG_RESULT($have_libdb)
        if test "$have_libdb" != "no"; then break; fi
        LIBS="$oldLIBS"
    done
    if test "$have_libdb" = "no" -o "$have_db_h" = "no"; then
        AC_MSG_RESULT([   * libdb not found - either install it, or use])
        AC_MSG_RESULT([   * \`configure --without-libdb' to disable the db])
        AC_MSG_RESULT([   * file cache of jigdo-file. If this check should not])
        AC_MSG_RESULT([   * have failed, force e.g. -ldb to be used with])
        AC_MSG_RESULT([   * \`configure --with-libdb=-ldb'])
        installDevel "libdb4" "libdb4"
        AC_MSG_ERROR(libdb not found.)
    fi
    AC_DEFINE(HAVE_LIBDB, 1) dnl make compilation fail if libdb not there
elif test "$jigdo_libdb" != "no" -a "$jigdo_libdb" != "NO"; then
    dnl Pass argument (e.g. "-ldb") to compiler
    LIBS="$jigdo_libdb $LIBS"
    AC_DEFINE(HAVE_LIBDB, 1)
else
    AC_DEFINE(HAVE_LIBDB, 0)
fi
dnl ________________________________________

AC_MSG_CHECKING(for value of --with-gui)
AC_ARG_WITH(gui,
    [  --with-gui              Build the jigdo GTK+ GUI application [auto]],
    jigdo_gui="$withval", jigdo_gui="auto")
AC_MSG_RESULT(\"$jigdo_gui\")
dnl ____________________

if test "$jigdo_gui" = "auto"; then
    jigdo_gui="yes"
    jigdo_gui_failed() {
    AC_MSG_RESULT([   * Disabling the build of the jigdo GUI application.])
    AC_MSG_RESULT([   * Use --with-gui=yes to force an attempt to build it.])
    jigdo_gui="no"
    }
else
    jigdo_gui_failed() { return; }
fi

if test "$jigdo_gui" = "yes"; then
    AC_CHECK_PROG(PKGCONFIG, pkg-config, yes, no)
    if test "$PKGCONFIG" = "no"; then
        AC_MSG_RESULT([   * GTK+ not installed, or pkg-config not in])
        AC_MSG_RESULT([   * \$PATH. Please install GTK+ 2.])
        installDevel "libgtk2.0" "libgtk2.0"
        jigdo_gui_failed
    fi
fi
GTKCFLAGS="# GUI application disabled by configure script"
GTKLIBS="$GTKCFLAGS"
GLIBLIBS="$GTKCFLAGS"
dnl LIBWWWCFLAGS="$GTKCFLAGS"
dnl LIBWWWLIBS="$GTKCFLAGS"
CURLCFLAGS="$GTKCFLAGS"
CURLLIBS="$GTKCFLAGS"
dnl The following adds flags for GTK+ and glib
if test "$jigdo_gui" = "yes"; then
    AC_MSG_CHECKING(for value of --with-pkg-config-prefix)
    AC_ARG_WITH(pkg-config-prefix,
    [  --with-pkg-config-prefix=PATH  When cross-compiling, specify prefix of
                          libraries/headers for target architecture [none]],
        jigdo_pkg_config_prefix="$withval", jigdo_pkg_config_prefix="")
    if test "$jigdo_pkg_config_prefix" = ""; then
        AC_MSG_RESULT(not set)
    else
        AC_MSG_RESULT(\"$jigdo_pkg_config_prefix\")
        jigdo_pkg_config_prefix="--define-variable=prefix=$jigdo_pkg_config_prefix"
    fi

    AC_MSG_CHECKING(for GTK+ 2.4.0 or later)
    gtk_ver="`pkg-config $jigdo_pkg_config_prefix gtk+-2.0 --modversion`"
    AC_MSG_RESULT($gtk_ver)
    if pkg-config gtk+-2.0 --atleast-version=2.4.0; then true; else
        AC_MSG_RESULT([   * Installed version of GTK+ is too old])
        jigdo_gui_failed
    fi
    if test "$is_windows" = yes; then gth="gthread-2.0"; else gth=""; fi
#gth="gthread-2.0"
    GTKCFLAGS="`pkg-config $jigdo_pkg_config_prefix gtk+-2.0 $gth --cflags 2>/dev/null`"
    GTKLIBS="`pkg-config $jigdo_pkg_config_prefix gtk+-2.0 $gth --libs 2>/dev/null`"
    GLIBLIBS="`pkg-config $jigdo_pkg_config_prefix glib-2.0 $gth --libs 2>/dev/null`"
fi

dnl dnl The following adds flags for libwww
dnl if test "$jigdo_gui" = "yes"; then
dnl     AC_MSG_CHECKING(for libwww 5.3.2 or later)
dnl     jigdo_libwwwver="`libwww-config --version 2>&1`"
dnl     AC_MSG_RESULT($jigdo_libwwwver)
dnl     case "$jigdo_libwwwver" in
dnl         dnl Extra [] around [0-9] style wildcards, or M4 swallows them
dnl         [[6-9]*|5.[4-9]*|5.[1-9][0-9]*|5.3.[2-9]*|5.3.[1-9][0-9]*])
dnl             LIBWWWCFLAGS="`libwww-config --cflags 2>/dev/null`"
dnl             LIBWWWLIBS="`libwww-config --libs 2>/dev/null`"
dnl             dnl Some versions of libwww-config include "-I/usr/include" in
dnl             dnl their --cflags output, which completely upsets some versions
dnl             dnl of GCC.
dnl             LIBWWWCFLAGS=`echo " $LIBWWWCFLAGS " | sed -e 's% -I/usr/include/* % %'`
dnl             dnl If we are cross-compiling for Windows and the wrong
dnl             dnl (ie host system's) libwww-config was executed above, we
dnl             dnl may need to turn -lz from its output into -lzdll. Yuk,
dnl             dnl that's hacky, but then it'll save people some trouble
dnl             case "$is_windows $LIBS " in
dnl                 yes*"-lzdll "*)
dnl                     LIBWWWLIBS=`echo " $LIBWWWLIBS " | sed -e 's% -lz % -lzdll %'`
dnl             esac
dnl             ;;
dnl         *)
dnl         AC_MSG_RESULT([   * libwww not installed, or the installed version])
dnl         AC_MSG_RESULT([   * is too old, or libwww-config is not in \$PATH.])
dnl         AC_MSG_RESULT([   * Please install libwww 5.3.2 or later, it is])
dnl         AC_MSG_RESULT([   * needed by the jigdo GUI application.])
dnl         installDevel "libwww" "w3c-libwww-devel/libopenssl0"
dnl         jigdo_gui_failed
dnl     esac
dnl fi

dnl The following adds flags for libcurl
dnl On Windows, no curl-config is supplied
if test "$jigdo_gui" = "yes"; then
  if test "$is_windows" = "no"; then
    AC_MSG_CHECKING(for libcurl 7.11.0 or later)
    set x `curl-config --version 2>&1`
    shift
    case "$1" in [[0-9]*]) ;; *) shift;; esac
    jigdo_curlver="$1"
    AC_MSG_RESULT($jigdo_curlver)
    case "$jigdo_curlver" in
        [[1-9][0-9].*|[8-9].*|7.1[1-9]*|7.[2-9][0-9]*])
            CURLCFLAGS="`curl-config --cflags 2>/dev/null`"
            CURLLIBS="`curl-config --libs 2>/dev/null`"
            ;;
        *)
        AC_MSG_RESULT([   * libcurl not installed, or the installed version])
        AC_MSG_RESULT([   * is too old, or curl-config is not in \$PATH.])
        AC_MSG_RESULT([   * Please install libcurl 7.11.0 or later, it is])
        AC_MSG_RESULT([   * needed by the jigdo GUI application.])
        installDevel "libcurl2" "libcurl2"
    esac
  else
    AC_CHECK_LIB(curl, curl_global_init, have_libcurl="-lcurl", have_libcurl="no")
    AC_CHECK_LIB(curldll, curl_global_init, have_libcurl="-lcurldll", )
    AC_CHECK_HEADER(curl/curl.h, have_curl_h="yes", have_curl_h="no")
    if test "$have_libcurl" = "no" -o "$have_curl_h" = "no"; then
        AC_MSG_RESULT([   * Please install libcurl 7.11.0 or later, it is])
        AC_MSG_RESULT([   * needed by the jigdo GUI application.])
    else
        CURLCFLAGS=""
	CURLLIBS="$have_libcurl"
    fi
  fi
fi

if test "$jigdo_gui" = "yes"; then IF_GUI=""; else IF_GUI="#"; fi
AC_SUBST(IF_GUI)
AC_SUBST(GTKCFLAGS)
AC_SUBST(GTKLIBS)
AC_SUBST(GLIBLIBS)
dnl AC_SUBST(LIBWWWCFLAGS)
dnl AC_SUBST(LIBWWWLIBS)
AC_SUBST(CURLCFLAGS)
AC_SUBST(CURLLIBS)
dnl ____________________

if test "$jigdo_gui" = "yes" && test ! -e "$srcdir/src/gtk/interface.hh"; then
  AC_CHECK_PROG(have_glade, glade-2, yes, no)
  if test "$have_glade" = "no"; then
    AC_MSG_RESULT([   * glade-2 was not found on your system. Please install])
    AC_MSG_RESULT([   * it, it is needed to compile the jigdo GUI (actually,])
    AC_MSG_RESULT([   * to create the file src/gtk/interface.hh)])
  fi
fi
dnl ____________________

AC_MSG_CHECKING(for value of --with-uint64)
AC_ARG_WITH(uint64,
    [  --with-uint64=TYPE      Specify unsigned type of at least 64 bits [auto]
                          (Replace spaces with underscores in TYPE)],
    jigdo_uint64="$withval", jigdo_uint64="auto")
AC_MSG_RESULT(\"$jigdo_uint64\")

AC_CACHE_CHECK(for unsigned long long, jigdo_cv_have_ulonglong,
    AC_TRY_COMPILE(, unsigned long long x = 0,
                   jigdo_cv_have_ulonglong=yes, jigdo_cv_have_ulonglong=no))
if test "$jigdo_cv_have_ulonglong" = "yes"; then
    AC_DEFINE(HAVE_UNSIGNED_LONG_LONG, 1)
else
    AC_DEFINE(HAVE_UNSIGNED_LONG_LONG, 0)
fi

if test "$jigdo_uint64" = auto; then
    AC_CHECK_SIZEOF(unsigned long, 0)
    AC_CHECK_SIZEOF(unsigned long long, 0)
    if test "$ac_cv_sizeof_unsigned_long" -ge 8; then
        jigdo_uint64='unsigned long'
    elif test "$ac_cv_sizeof_unsigned_long_long" -ge 8; then
        jigdo_uint64='unsigned long long'
    else
        #if test "$jigdo_cv_have_ulonglong" = "yes"; then
            jigdo_uint64='unsigned long long'
        #else
        #    jigdo_uint64='unsigned long'
        #fi
        AC_MSG_RESULT([   * Could not find an unsigned type with at least 64])
        AC_MSG_RESULT([   * bits (or maybe char has >8 bits?) - defaulting to])
        AC_MSG_RESULT([   * \`$jigdo_uint64'. You may want to override])
        AC_MSG_RESULT([   * this using --with-uint64=TYPE])
    fi
elif test "$jigdo_uint64" = yes -o "$jigdo_uint64" = no; then
    AC_MSG_ERROR(Invalid argument to --with-uint64 option)
else
    jigdo_uint64=`echo $jigdo_uint64 | sed -e 's/_/ /g'`
fi
AC_MSG_CHECKING(for 64 bit unsigned integer type)
AC_MSG_RESULT($jigdo_uint64)
AC_DEFINE_UNQUOTED(TYPE_UINT64, $jigdo_uint64)

AC_CACHE_CHECK(for operator<<(uint64),
               jigdo_cv_prog_cxx_outuint64,
    AC_TRY_COMPILE(
    [ #include <iostream>
      namespace std { }
      using namespace std; ],
    [ $jigdo_uint64 x;
      cout << x << endl; ],
    jigdo_cv_prog_cxx_outuint64="yes", jigdo_cv_prog_cxx_outuint64="no")
)
if test "$jigdo_cv_prog_cxx_outuint64" = "yes"; then
    AC_DEFINE(HAVE_OUTUINT64, 1)
else
    AC_DEFINE(HAVE_OUTUINT64, 0)
fi

AC_CACHE_CHECK([for string::compare(size_t,size_t,string,size_t,size_t)],
               jigdo_cv_prog_cxx_stringcmp,
    AC_TRY_COMPILE(
    [ #include <string>
      namespace std { }
      using namespace std; ],
    [ string s, t;
      s.compare(0, std::string::npos, t, 0, std::string::npos); ],
    jigdo_cv_prog_cxx_stringcmp="yes", jigdo_cv_prog_cxx_stringcmp="no")
)
if test "$jigdo_cv_prog_cxx_stringcmp" = "yes"; then
    AC_DEFINE(HAVE_STRINGCMP, 1)
else
    AC_DEFINE(HAVE_STRINGCMP, 0)
fi
AC_CACHE_CHECK([for string::compare(size_t,size_t,const char*,size_t)],
               jigdo_cv_prog_cxx_stringstrcmp,
    AC_TRY_COMPILE(
    [ #include <string>
      namespace std { }
      using namespace std; ],
    [ string s; const char* t = "x";
      s.compare(0, std::string::npos, t, 1); ],
    jigdo_cv_prog_cxx_stringstrcmp="yes", jigdo_cv_prog_cxx_stringstrcmp="no")
)
if test "$jigdo_cv_prog_cxx_stringstrcmp" = "yes"; then
    AC_DEFINE(HAVE_STRINGSTRCMP, 1)
else
    AC_DEFINE(HAVE_STRINGSTRCMP, 0)
fi
dnl ____________________

dnl Checks for library functions.
AC_CHECK_FUNCS(lstat truncate ftruncate mmap memcpy fileno snprintf \
               _snprintf setenv)

dnl Check whether reading width of TTY via ioctl() works
AC_CACHE_CHECK([for TIOCGWINSZ ioctl],
               jigdo_cv_ioctl_winsz,
    AC_TRY_COMPILE(
        [ #include <stdio.h>
          #include <sys/ioctl.h>],
        [ struct winsize w; int i;
          ioctl(fileno(stdout), TIOCGWINSZ, &w);
          i = w.ws_col; ],
        jigdo_cv_ioctl_winsz="yes", jigdo_cv_ioctl_winsz="no")
)
if test "$jigdo_cv_ioctl_winsz" = "yes"; then
    AC_DEFINE(HAVE_IOCTL_WINSZ, 1)
elif test "$is_windows" = "no"; then
    AC_MSG_RESULT([   * jigdo-file progress reports will not be formatted])
    AC_MSG_RESULT([   * nicely, because no way has been found to determine])
    AC_MSG_RESULT([   * the screen width in characters.])
    AC_DEFINE(HAVE_IOCTL_WINSZ, 0)
else
    AC_DEFINE(HAVE_IOCTL_WINSZ, 0)
fi

dnl Check whether getopt_long is present
AC_CACHE_CHECK([for getopt_long in <getopt.h>],
               jigdo_cv_func_getopt_long,
    AC_TRY_LINK(
        [ #include <getopt.h> ],
        [[ static const struct option longopts[] = { { 0, 0, 0, 0 } };
          int c = getopt_long(0, 0, "", longopts, 0); ]],
        jigdo_cv_func_getopt_long="yes", jigdo_cv_func_getopt_long="no"
    )
)
if test "$jigdo_cv_func_getopt_long" = "yes"; then
    AC_DEFINE(HAVE_GETOPT_LONG, 1)
else
    AC_DEFINE(HAVE_GETOPT_LONG, 0)
fi

dnl Check whether uname() is present
AC_CACHE_CHECK([for uname in <sys/utsname.h>],
               jigdo_cv_func_uname,
    AC_TRY_LINK(
        [ #include <sys/utsname.h> ],
        [ struct utsname ubuf; uname(&ubuf); ],
        jigdo_cv_func_uname="yes", jigdo_cv_func_uname="no"
    )
)
if test "$jigdo_cv_func_uname" = "yes"; then
    AC_DEFINE(HAVE_UNAME, 1)
else
    AC_DEFINE(HAVE_UNAME, 0)
fi

dnl On native Windows (MinGW32), there is no snprintf, just _snprintf
if test "$ac_cv_func_snprintf" = no -a "$ac_cv_func__snprintf" = "yes"; then
    AC_DEFINE(snprintf, _snprintf)
fi
dnl On native Windows, must use _stati64 for big file support
AC_CACHE_CHECK(for _stati64, jigdo_cv_func__stati64,
    AC_TRY_COMPILE([ #include <sys/types.h>
                     #include <sys/stat.h>],
                   [ struct __stati64 buf;
                     int x = _stati64("file", &buf);],
                   jigdo_cv_func__stati64=yes, jigdo_cv_func__stati64=no))
if test "$is_windows" = "yes" -a "$ac_cv_func__stati64" = "yes"; then
    AC_DEFINE(stat, _stati64)
fi
dnl ______________________________________________________________________

dnl Check for Native Language Support (gettext)
AC_MSG_CHECKING(for value of --enable-nls)
AC_ARG_ENABLE(nls,
    [  --disable-nls           Disable multi-language support],
    USE_NLS=$enableval, USE_NLS=yes)
AC_MSG_RESULT(\"$USE_NLS\")
AC_SUBST(USE_NLS)
CATALOGS=""
if test "$USE_NLS" = "yes"; then
    oldLIBS="$LIBS"
    AC_MSG_CHECKING(for dgettext in <libintl.h> and libc)
    AC_TRY_LINK([#include <libintl.h>], dgettext(0, 0),
                have_intl="yes", have_intl="no");
    AC_MSG_RESULT($have_intl)
    if test "$have_intl" = "no"; then
        LIBS="$LIBS -lintl";
        AC_MSG_CHECKING(for dgettext in <libintl.h> and -lintl)
        AC_TRY_LINK([#include <libintl.h>], dgettext(0, 0),
                    have_intl="yes", have_intl="no");
        AC_MSG_RESULT($have_intl)
    fi
    if test "$have_intl" = "no"; then
        AC_MSG_RESULT([   * Disabling gettext support - jigdo will not be])
        AC_MSG_RESULT([   * able to display translated status/error])
        AC_MSG_RESULT([   * messages instead of the default English ones.])
        USE_NLS="no"
        LIBS="$oldLIBS"
    fi
fi
if test "$USE_NLS" = "yes"; then
    AC_DEFINE(ENABLE_NLS, 1)

    AC_MSG_CHECKING(for catalogs to be installed)
    ALL_LINGUAS=`(cd "$srcdir/po" && echo *.po *.pox "") \
        | sed -e "s/\**\.pox* / /g"`
    if test "$LINGUAS"; then
        NEW_LINGUAS=""
        for provided in $ALL_LINGUAS; do
            x=""
            for wanted in $LINGUAS; do
                case "$provided" in "$wanted"*) x=" $provided";; esac
                case "$wanted" in "$provided"*) x=" $provided";; esac
            done
            NEW_LINGUAS="$NEW_LINGUAS$x"
        done
        LINGUAS="$NEW_LINGUAS"
        AC_MSG_RESULT(LINGUAS:$LINGUAS)
    else
        LINGUAS="$ALL_LINGUAS"
        AC_MSG_RESULT($LINGUAS)
    fi
    for x in $LINGUAS; do CATALOGS="$CATALOGS $x.gmo"; done
else
    AC_DEFINE(ENABLE_NLS, 0)
fi
AC_SUBST(CATALOGS)
dnl ______________________________________________________________________

dnl Checks for other --enable-xxx, --with-xxx switches

dnl AC_MSG_CHECKING(for value of --enable-libwww-hacks)
dnl AC_ARG_ENABLE(libwww-hacks,
dnl     [  --disable-libwww-hacks  Don't compile fixed versions of some libwww
dnl                           code (Try this if you see link errors/crashes)],
dnl     jigdo_libwww_hacks=$enableval, jigdo_libwww_hacks=yes)
dnl AC_MSG_RESULT(\"$jigdo_libwww_hacks\")
dnl if test "$jigdo_libwww_hacks" = yes; then
dnl     IF_LIBWWW_HACKS="" # enable hacks
dnl elif test "$jigdo_libwww_hacks" != no; then
dnl     AC_MSG_ERROR(Invalid argument to --enable-libwww-hacks option)
dnl else
dnl     IF_LIBWWW_HACKS="#" # disable hacks, by commenting out Makefile line
dnl fi
dnl AC_SUBST(IF_LIBWWW_HACKS)
dnl ______________________________________________________________________

dnl Do this at end in order not to influence other compile tests which
dnl might return warnings. Turning the warnings into errors would make
dnl the tests fail.
dnl if test "$jigdo_debug" = "yes"; then
dnl     #if test "$GCC" = yes; then CFLAGS="$CFLAGS -Werror"; fi
dnl     if test "$GXX" = yes; then CXXFLAGS="$CXXFLAGS -Werror"; fi
dnl fi
if test "$GXX" = "yes"; then
    CFLAGS="-Wall $CFLAGS -W"
    CXXFLAGS="$CXXFLAGS -Wall -W -Wpointer-arith -Wconversion -Woverloaded-virtual"
fi
dnl ______________________________________________________________________

if test "$cross_compiling" = yes; then
  IF_CROSSCOMPILING=""
  IFNOT_CROSSCOMPILING="#"
else
  IF_CROSSCOMPILING="#"
  IFNOT_CROSSCOMPILING=""
fi
AC_SUBST(IF_CROSSCOMPILING)
AC_SUBST(IFNOT_CROSSCOMPILING)
dnl ______________________________________________________________________

dnl Touch TAGS to prevent it from being recreated when a normal user
dnl builds the program. TAGS should only be updated once changes are
dnl made to the source.
touch "$srcdir/src/TAGS"

SUBDIRS="glibcurl gtk job net util"
SRC_MAKEDEPS="$srcdir/src/Makedeps"
dnl When bootstrapping from CVS, we need to make sure that correct
dnl dependencies cause gtk/interface.hh to be created
if test -e "$SRC_MAKEDEPS"; then true; else
  AC_MSG_RESULT(Creating $SRC_MAKEDEPS)
  touch "$SRC_MAKEDEPS"
  echo >"$srcdir/src/gtk/interface.hh"
  (cd "$srcdir/src"
   find . -type f '(' -name '*.cc' -o -name '*.c' ')' \
   | xargs "$ac_cv_prog_AWK" -f ../scripts/depend.awk "$srcdir" $SUBDIRS -
  )
  rm -f "$SRC_MAKEDEPS.bak" "$srcdir/src/gtk/interface.hh"
fi
AC_SUBST_FILE(SRC_MAKEDEPS)

dnl Create subdirs in src directory
for dir in $SUBDIRS; do
    test ! -d "src/$dir" && mkdir "src/$dir"
done
AC_OUTPUT(Makefile doc/Makefile src/Makefile po/Makefile)

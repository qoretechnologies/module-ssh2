# Process this file with autoconf to produce a configure script.

# AC_PREREQ(2.59)
AC_INIT([qore-ssh2-module], [1.5.0],
        [David Nichols <david(a)qore(dot)org>, Wolfgang Ritzinger <aargon(a)rat(dot)at>],
        [qore-ssh2-module])
AM_INIT_AUTOMAKE([no-dist-gzip dist-bzip2])
AC_CONFIG_HEADER([config.h])

AC_LANG(C++)
AC_PROG_CXX
AC_PROG_CC
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL
AC_PROG_INSTALL

AC_CONFIG_MACRO_DIR([m4])

# set system variables
OS=`uname -s`
if test "$OS" = "HP-UX"; then
   if test "$host_cpu" = "ia64"; then
      ARCH=itanium
   else
      ARCH=PA-RISC
   fi
else
   ARCH=`uname -p`
   if test "$ARCH" = unknown; then
      ARCH=`uname -m`
   fi
   if test $ARCH = i686 -o $ARCH = i586 -o $ARCH = athlon ; then
      ARCH=i386
   fi
fi

# set find prefix
if test -n "$prefix" -a "$prefix" != "NONE"; then
   find_prefix="$prefix"
fi

#AC_CANONICAL_HOST
# get CPU family
case "${host_cpu}" in
   i[[3456]]86) cpu_family=i386 ;;
   amd64)       cpu_family=x86_64 ;;
   *)           cpu_family=${host_cpu} ;;
esac

# set OS-specific variables
SHLIB_SUFFIX=so
#echo host_os=${host_os}sss
case "${host_os}" in
     *linux*)   # set compile flags for Linux
		CPPFLAGS="${CPPFLAGS} -D_GNU_SOURCE"
		SHLIB_SUFFIX=so
		AC_DEFINE(LINUX, 1, if compiling on Linux)
		;;
     *solaris*) # for checking for some network functions on Solaris
		LDFLAGS="$LDFLAGS -lnsl"
		# assume we are using CC if not using g++, add -norunpath to link flags in this case
		if test "$GXX" != "yes"; then
		   CXXFLAGS="${CXXFLAGS} -mt"
		   LDFLAGS="$LDFLAGS -norunpath -mt"
		fi
		AC_DEFINE(SOLARIS, 1, if compiling on Solaris)
		# add -ldl to libs if on Solaris 8
		if test "${host_os}" = "solaris2.8"; then
		   OTHER_LIBS="$OTHER_LIBS -ldl"
		fi
		solaris=yes
	        ;;
     *darwin*)  SHLIB_SUFFIX=dylib
		# libtool 1.5.* is creating modules on Darwin 8.* (OS/X 10.4.*) with a .so suffix for some reason
		MODULE_SUFFIX=so
		AC_DEFINE(DARWIN, 1, [if compiling on Darwin])
		darwin=true
		;;
     *hpux*)    if test "$host_cpu" = "ia64"; then
     		   SHLIB_SUFFIX=so
		else  # assume hppa (PA-RISC)
     		   SHLIB_SUFFIX=sl
		   AC_DEFINE(SHLIB_SUFFIX, "sl", shared library suffix)
		fi
		# add "-AA" and -D__STDC_EXT__ to aCC commandline to enable a clean compile
		if test -n "`echo $CXX|grep aCC`"; then
		   CXXFLAGS="$CXXFLAGS -AA -D__STDC_EXT__ -D_RWSTD_MULTI_THREAD"
		fi
		AC_DEFINE(HPUX, 1, if compiling on HP-UX)
		;;
     *cygwin*)  SHLIB_SUFFIX=dll
     		CPPFLAGS=-mwin32
		windows=yes
     		;;
     *mingw32*) SHLIB_SUFFIX=dll
		windows=yes
		mingw=yes
		#CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0501"
		LDFLAGS="$LDFLAGS -lwsock32 -lws2_32 -lgdi32 -lz -lsecur32 -lpthread"
		#make_dll=yes
		CPPFLAGS="$CPPFLAGS -DBUILDING_DLL=1"
		MODULE_LDFLAGS="-no-undefined"
		OS=Windows
		# set LIB_PREFIX if not already set
		if test -z "$LIB_PREFIX"; then
   		   AC_MSG_CHECKING([for mingw32 library prefix])
		   cc_loc=`which $CXX|head -1`
		   mingw_base=`dirname $cc_loc|sed 's/\/@<:@^/@:>@*$//'`
		   cc_targ=`echo $CXX|sed s/-g++$//`
		   if test -d $mingw_base/$cc_targ; then
		      LIB_PREFIX=$mingw_base/$cc_targ
		      AC_MSG_RESULT([$LIB_PREFIX])
		   else
		      AC_MSG_RESULT([not found])
		   fi
		fi
		;;
     *mingw64*) SHLIB_SUFFIX=dll
		windows=yes
		mingw=yes
		#CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0501"
		LDFLAGS="$LDFLAGS -lwsock32 -lws2_32 -lgdi32 -lz -lsecur32 -lpthread"
		#make_dll=yes
		CPPFLAGS="$CPPFLAGS -DBUILDING_DLL=1"
		MODULE_LDFLAGS="-no-undefined"
		OS=Windows
		# set LIB_PREFIX if not already set
		if test -z "$LIB_PREFIX"; then
   		   AC_MSG_CHECKING([for mingw64 library prefix])
		   cc_loc=`which $CXX|head -1`
		   mingw_base=`dirname $cc_loc|sed 's/\/@<:@^/@:>@*$//'`
		   cc_targ=`echo $CXX|sed s/-g++$//`
		   if test -d $mingw_base/$cc_targ; then
		      LIB_PREFIX=$mingw_base/$cc_targ
		      AC_MSG_RESULT([$LIB_PREFIX])
		   else
		      AC_MSG_RESULT([not found])
		   fi
		fi
		;;
     *)         AC_WARN(Unknown platform ${host_os})
                ;;
esac
AC_SUBST(SHLIB_SUFFIX)
if test -z "$MODULE_SUFFIX" ; then
   MODULE_SUFFIX=$SHLIB_SUFFIX
fi
AC_SUBST(MODULE_SUFFIX)

AC_ARG_ENABLE([64bit],
  [AS_HELP_STRING([--enable-64bit],
		  [enable 64bit support (default: auto)])],
  [case "${enable_64bit}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_64bit} for --enable-64bit) ;;
      esac],
  [enable_64bit=auto])

if test "$enable_64bit" = "auto"; then
   # turn on 64-bit by default if compiling on a 64-bit platform
   case "${host_cpu}" in
       *64*)
                 enable_64bit=yes
                 ;;
       s390x)
                 enable_64bit=yes
                 ;;
       *)        case "${host_os}" in
                     darwin*)   mv=`uname -r|cut -f1 -d.`
 		                if test $mv -ge 10 -a "${host_cpu}" = "i386"; then
		         	    enable_64bit=yes
		      		fi
		     ;;
		     solaris*)  if test "${host_cpu}" = "i386"; then
		     	            # if we are running on a 64-bit machine, then enable a 64-bit build
				    if test -n "`isainfo -v 2>/dev/null| grep 64-bit`"; then
		         	        enable_64bit=yes
		     		    fi
		     	        fi
		     ;;
       		 esac
		 if test "$enable_64bit" = "auto"; then
		     enable_64bit=no
		 fi
                 ;;
    esac
fi

# allow users to explicitly set libsuffix
AC_ARG_WITH([libsuffix],
  [AS_HELP_STRING([--with-libsuffix@<:@=LIBSUFFIX@:>@],
                  [explitily set libsuffix])],
  [LIBSUFFIX=$with_libsuffix; LIBSUFFIX_SET=yes;AC_SUBST(LIBSUFFIX)],
  [])

# see if we know how to set flags properly for different hosts and architectures
# FIXME: add test on HP-UX if the compiler can generate the appropriate binaries
if test "$enable_64bit" = "yes"; then
   case "${host_os}" in
      *linux*)	if test "$GXX" = "yes" -a "$ARCH" != "aarch64"; then
                   CXXFLAGS="$CXXFLAGS -m64"
                fi
                # multiarch
                if test "$LIBSUFFIX_SET" != "yes"; then
                    if test -d /usr/lib64; then
                        LIBSUFFIX=64
                    fi
                    if test "$GXX" = "yes"; then
                        multiarch=`${CXX} --print-multiarch 2> /dev/null`
                        if test -n "${multiarch}" -a -d /usr/lib/${multiarch}; then
                            LIBSUFFIX=/${multiarch}
                        fi
                    fi
                    AC_SUBST(LIBSUFFIX)
                fi
                ;;
      *hpux*)	if test "$host_cpu" = "ia64"; then
                    if test -n "`echo $CXX|grep aCC`"; then
                        CXXFLAGS="$CXXFLAGS +DD64"
                    elif test "$GXX" = "yes"; then
                        CXXFLAGS="$CXXFLAGS -mlp64"
                    fi
                else
		    if test -n "`echo $CXX|grep aCC`"; then
		        CXXFLAGS="$CXXFLAGS +DA2.0W"
		    elif test "$GXX" = "yes"; then
			CXXFLAGS="$CXXFLAGS -march=2.0"
		    fi
		fi
		;;
     *solaris*) if test "$GXX" = "yes"; then
		    if test "$host_cpu" = "sparc"; then
		        CXXFLAGS="$CXXFLAGS -mcpu=ultrasparc3 -m64"  # NOTE we compile for minimum ultrasparc 3 in 64-bit mode
		    else
		        CXXFLAGS="$CXXFLAGS -m64"
		    fi
		else
		    CXXFLAGS="$CXXFLAGS -xarch=generic64"
		fi
		;;
   esac
   bits=64
else
   case "${host_os}" in
     *linux*)	if test "$GXX" = "yes"; then
                    case "$host_cpu" in
                        arm*) ;;
                        *) CXXFLAGS="$CXXFLAGS -m32" ;;
                    esac
                fi
                # multiarch
                if test "$LIBSUFFIX_SET" != "yes"; then
                    if test "$host_cpu" = "x86_64" -a -d /usr/lib32; then
                        LIBSUFFIX=32
                    fi
                    if test "$GXX" = "yes"; then
                        multiarch=`${CXX} --print-multiarch 2> /dev/null`
                        if test -n "${multiarch}" -a -d /usr/lib/${multiarch}; then
                            LIBSUFFIX=/${multiarch}
                        fi
                    fi
                    AC_SUBST(LIBSUFFIX)
                fi
                ;;
     *hpux*)	if test "$host_cpu" != "ia64"; then
		    if test "$GXX" = "yes"; then
		        CXXFLAGS="$CXXFLAGS -march=2.0"
		    elif test -n "`echo $CXX|grep aCC`" -a "$host_cpu" != "ia64"; then
		        CXXFLAGS="$CXXFLAGS +DA2.0N"   # NOTE we compile for PA-RISC 2.0 32-bit, not PA-RISC 1.1
		    fi
		fi
		;;
     *solaris*) if test "$GXX" = "yes"; then
		    CXXFLAGS="$CXXFLAGS -m32"
		else
		    CXXFLAGS="$CXXFLAGS -xarch=generic"
		fi
		;;
   esac
   if test "$host_cpu" = "x86_64"; then
      cpu_family=i386
   fi
   bits=32
fi

# test for C++11
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])
if test "$HAVE_CXX11" = "1"; then
    AM_CXXFLAGS=-std=gnu++11
fi

# checks for how to build threads
ACX_PTHREAD
if test "$acx_pthread_ok" = "no"; then
   AC_MSG_ERROR(POSIX threads do not seem to be supported on this platform, aborting)
fi
CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
#LDFLAGS="$LDFLAGS $PTHREAD_LIBS"

# see if the libssh2 library is compiled with support for wincng
AC_ARG_WITH([wincng],
  [AS_HELP_STRING([--with-wincng],
                  [if libssh2 is compiled with wincng instead of openssl])],
  [],
  [with_wincng="$LIBSSH2_WINCNG"])

if test "$with_wincng" = "yes" -o "$with_wincng" = "true" -o "$with_wincng" = "on" -o "$with_wincng" = "1"; then
  disable_openssl=1

  case "${host_os}" in
     *mingw32*) LDFLAGS="$LDFLAGS -lbcrypt -lcrypt32"
     ;;
  esac
fi

# allow the user to set a generic search prefix for libraries, includes, etc
AC_ARG_WITH([lib-prefix],
  [AS_HELP_STRING([--with-lib-prefix@<:@=DIR@:>@],
                  [generic search prefix for libraries, includes, etc])],
  [if test ! -d "${with_lib_prefix}"; then AC_MSG_ERROR([bad value ${with_lib_prefix} for --with-lib-prefix]); unset with_lib_prefix; fi],
  [with_lib_prefix="$LIB_PREFIX"])

AC_ARG_ENABLE([single-compilation-unit],
  [AS_HELP_STRING([--enable-single-compilation-unit],
                  [enable compilation as a single unit (default: on)])],
  [case "${enable_single_compilation_unit}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_single_compilation_unit} for --enable-single-compilation-unit) ;;
      esac],
  [enable_single_compilation_unit=yes])

set_openssl_cppflags() {
    if test "$1" != "/usr/include"; then
        OPENSSL_CPPFLAGS=-I$1
    fi
}

find_openssl() {
    dir="$1"
    if test -z "$dir" -o "$dir" = "/"; then
        if test -h /lib${LIBSUFFIX}; then
	    dir=/usr
	    lib=/usr/lib${LIBSUFFIX}
	    inc=/usr/include
        else
            dir="/"
   	    lib=/lib${LIBSUFFIX}
	    inc=/usr/include
        fi
    else
        lib="$dir/lib${LIBSUFFIX}"
        inc="$dir/include"
    fi
    # if we only want the static openssl libraries
    if test -n "$openssl_static"; then
    	if test -f "$lib/libssl.a" -a -f "$lib/libcrypto.a"; then
	    OPENSSL_LDFLAGS="$lib/libssl.a $lib/libcrypto.a"
	    msg_lib="$lib (forced static)"
    	elif test "$bits" = 64 -a -f "$lib/64/libssl.a" -a -f "$lib/64/libcrypto.a"; then
	    OPENSSL_LDFLAGS="$lib/64/libssl.a $lib/64/libcrypto.a"
	    msg_lib="$lib/64 (forced static)"
    	fi
    else
        if test -f "$lib/libssl.${SHLIB_SUFFIX}" -a -f "$lib/libcrypto.${SHLIB_SUFFIX}"; then
	    if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
       	        OPENSSL_LDFLAGS="-L$lib"
    	    fi
	    OPENSSL_LDFLAGS="$OPENSSL_LDFLAGS -lssl -lcrypto"
	    msg_lib="$lib (shared)"
    	elif test -f "$lib/libssl.a" -a -f "$lib/libcrypto.a"; then
	    if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
       	        OPENSSL_LDFLAGS="-L$lib"
    	    fi
	    OPENSSL_LDFLAGS="$OPENSSL_LDFLAGS -lssl -lcrypto"
	    msg_lib="$lib (static)"
        elif test "$bits" = 64 -a -f "$lib/64/libssl.${SHLIB_SUFFIX}" -a -f "$lib/64/libcrypto.${SHLIB_SUFFIX}"; then
	    OPENSSL_LDFLAGS="-L$lib/64 -lssl -lcrypto"
	    msg_lib="$lib/64 (shared)"
    	elif test "$bits" = 64 -a -f "$lib/64/libssl.a" -a -f "$lib/64/libcrypto.a"; then
	    OPENSSL_LDFLAGS="-L$lib -lssl -lcrypto"
	    msg_lib="$lib (static)"
    	fi
    fi
    if test -z "$OPENSSL_LDFLAGS"; then
        unset msg_lib
        return
    fi
    # try to find include files
    if test -f "$inc/openssl/ssl.h"; then
        msg_inc="$inc/openssl"
        set_openssl_cppflags $inc
    else
        unset OPENSSL_LDFLAGS
        unset msg_lib
	return
    fi

    AC_MSG_RESULT([libs: $msg_lib includes: $msg_inc])

    AC_SUBST(OPENSSL_LDFLAGS)
    AC_SUBST(OPENSSL_CPPFLAGS)
}

if test -z "$disable_openssl"; then
  # see if we can figure out where the openssl library is
  AC_ARG_WITH([openssl],
    [AS_HELP_STRING([--with-openssl@<:@=DIR@:>@],
                    [openssl directory])],
    [if test "${with_openssl}" != "no" -a ! -d "${with_openssl}"; then AC_MSG_ERROR(bad value ${with_openssl} for --with-openssl); unset with_openssl; fi],
    [with_openssl="$OPENSSL_DIR"])

  if test "$with_openssl" = "no"; then
    disable_openssl=yes
  fi
fi

if test -z "$disable_openssl"; then
  AC_MSG_CHECKING([for openssl libraries and header files])
  for dir in "${with_openssl}" "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/openssl /usr/local/openssl /opt/local /sw /usr/sfw /opt/sfw; do
    find_openssl $dir
    if test -n "$OPENSSL_LDFLAGS"; then
      break
    fi
  done
  if test -z "$OPENSSL_LDFLAGS"; then
    AC_MSG_ERROR([no openssl library found])
  else
    # check for openssl const correctness (later versions have it)
    SAVE_CPPFLAGS="$CPPFLAGS"
    SAVE_LDFLAGS="$LDFLAGS"
    CPPFLAGS="$CPPFLAGS $OPENSSL_CPPFLAGS"
    LDFLAGS="$LDFLAGS $OPENSSL_LDFLAGS"

    AC_MSG_CHECKING([const correctness in openssl asn1 header])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <openssl/asn1.h>
         ]],[[
const unsigned char *p;
d2i_ASN1_type_bytes(0, &p, 0, 0);
        ]])],openssl_const_ok=yes, openssl_const_ok=no)

    CPPFLAGS="$SAVE_CPPFLAGS"
    LDFLAGS="$SAVE_LDFLAGS"

    if test "${openssl_const_ok}" = "yes"; then
      AC_MSG_RESULT([ok])
      AC_DEFINE(HAVE_OPENSSL_CONST, 1, Define if openssl headers correctly use const)
    else
      AC_MSG_RESULT([old style, not const correct])
    fi
  fi
fi

AC_ARG_WITH([doxygen],
    [AS_HELP_STRING([--with-doxygen@<:@=PATH@:>@],
                    [path to doxygen binary])],
    [if test "${with_doxygen}" != "no" -a ! -x "${with_doxygen}"; then AC_MSG_ERROR(${with_doxygen} is not an executable in --with-doxygen); unset with_doxygen; fi ],
    [with_doxygen=auto])

if test "${with_doxygen}" = "auto"; then
   AC_MSG_CHECKING([for doxygen to build API documentation])
   # check for doxygen
   with_doxygen=`which doxygen 2>/dev/null | grep -v "no doxygen in.*"`
   if test -n "$with_doxygen"; then
      doxy_version=`$with_doxygen --version`
      AC_MSG_RESULT([found $with_doxygen $doxy_version])
   else
      AC_MSG_RESULT([not found])
   fi
fi

if test -n "${with_doxygen}"; then
   AC_SUBST(DOXYGEN_CMD, $with_doxygen)
fi

################
# module stuff #
################

find_libssh2() {
    a="$1"
    if test \( "$a" = "/" -o -z "$a" \) -a -h "$a/lib${LIBSUFFIX}"; then
        a=/usr
    fi
    lib=$a/lib

    # if we only want the static libssh2 libraries
    if test -n "$libssh2_static"; then
        if test -f "$lib${LIBSUFFIX}/libssh2.a"; then
            LIBSSH2_LDFLAGS="$lib${LIBSUFFIX}/libssh2.a"
            AC_MSG_RESULT([$lib${LIBSUFFIX} (forced static)])
        elif test -f "$lib/libssh2.a"; then
            LIBSSH2_LDFLAGS="$lib/libssh2.a"
            AC_MSG_RESULT([$lib (forced static)])
        fi
    else
        if test -f "$lib${LIBSUFFIX}/libssh2.${SHLIB_SUFFIX}"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                LIBSSH2_LDFLAGS="-L$lib${LIBSUFFIX}"
            fi
            LIBSSH2_LDFLAGS="$LIBSSH2_LDFLAGS -lssh2"
            AC_MSG_RESULT([$lib (shared)])
        elif test -f "$lib/libssh2.${SHLIB_SUFFIX}"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                LIBSSH2_LDFLAGS="-L$lib"
            fi
            LIBSSH2_LDFLAGS="$LIBSSH2_LDFLAGS -lssh2"
            AC_MSG_RESULT([$lib (shared)])
        elif test -f "$lib${LIBSUFFIX}/libssh2.a"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                LIBSSH2_LDFLAGS="-L$lib${LIBSUFFIX}"
            fi
            LIBSSH2_LDFLAGS="$LIBSSH2_LDFLAGS -lssh2"
            AC_MSG_RESULT([$lib (static)])
        elif test -f "$lib/libssh2.a"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                LIBSSH2_LDFLAGS="-L$lib"
            fi
            LIBSSH2_LDFLAGS="$LIBSSH2_LDFLAGS -lssh2"
            AC_MSG_RESULT([$lib (static)])
        fi
    fi
    if test -z "$LIBSSH2_LDFLAGS"; then
        return
    fi

    AC_MSG_CHECKING([for libssh2 include files])
    # try to find include files
    if test "$a" = "/" -o -z "$a"; then
        inc=/usr/include
    else
        inc=$a/include
    fi
    if test -f "$inc/libssh2.h" -a -f "$inc/libssh2_sftp.h"; then
        LIBSSH2_CPPFLAGS="-I$inc"
        libssh2h=$inc/libssh2.h
        libssh2_version=$( grep "^\#define LIBSSH2_VERSION " $libssh2h | cut -d'"' -f2 )
        AC_MSG_RESULT([found: $inc])
    else
        AC_MSG_ERROR([not found])
    fi

    AC_SUBST(LIBSSH2_LDFLAGS)
    AC_SUBST(LIBSSH2_CPPFLAGS)
}

# see if we can figure out where the libssh2 library is
AC_ARG_WITH([libssh2-dir],
  [AS_HELP_STRING([--with-libssh2-dir@<:@=DIR@:>@],
                  [libssh2 directory])],
  [if test ! -d "${with_libssh2_dir}"; then AC_MSG_ERROR(bad value ${with_libssh2_dir} for --with-libssh2-dir); unset with_libssh2_dir; fi],
  [with_libssh2_dir="$LIBSSH2_DIR"])

AC_MSG_CHECKING([for libssh2 libraries and header files])
for dir in "${with_libssh2_dir}" "${with_lib_prefix}" "${find_prefix}" /usr /usr/local /opt/libssh2 /opt/local /sw /usr/sfw /opt/sfw /opt/gnu /opt/qore /usr/local/qore ; do
    find_libssh2 $dir
    if test -n "$LIBSSH2_LDFLAGS"; then
        break
    fi
done
if test -z "$LIBSSH2_LDFLAGS"; then
    AC_MSG_ERROR([no libssh2 libraries found])
else
    AC_MSG_RESULT([libssh2 version ${libssh2_version} OK (include: ${LIBSSH2_CPPFLAGS})])
    AC_SUBST(LIBSSH2_LDFLAGS)
    AC_SUBST(LIBSSH2_CPPFLAGS)

    AC_MSG_CHECKING([for libssh2_session_handshake()])
    if test -n "`grep libssh2_session_handshake $libssh2h 2>/dev/null`"; then
       AC_MSG_RESULT([yes])
       AC_DEFINE(HAVE_LIBSSH2_SESSION_HANDSHAKE, 1, [define if libssh2_session_handshake() is available])
    else
       AC_MSG_RESULT([no])
    fi

    AC_MSG_CHECKING([for libssh2_keepalive_config()])
    if test -n "`grep libssh2_keepalive_config $libssh2h 2>/dev/null`"; then
       AC_MSG_RESULT([yes])
       AC_DEFINE(HAVE_LIBSSH2_KEEPALIVE_CONFIG, 1, [define if libssh2_keepalive_config() is available])
    else
       AC_MSG_RESULT([no])
    fi
fi

#################
# qore includes #
#################

set_qore_cppflags() {
    QORE_INC_DIR=$1
    if test "$1" != "/usr/include"; then
        QORE_CPPFLAGS=-I$1
    fi
}

find_qore() {
    a="$1"
    if test \( "$a" = "/" -o -z "$a" \) -a -h "$a/lib${LIBSUFFIX}"; then
        a=/usr
    fi
    lib=$a/lib${LIBSUFFIX}

    # if we only want the static qore libraries
    if test -n "$qore_static"; then
        if test -f "$lib/libqore.a"; then
            QORE_LDFLAGS="$lib/libqore.a"
            AC_MSG_RESULT([$lib (forced static)])
        fi
    else
        if test -f "$lib/libqore.${SHLIB_SUFFIX}"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                QORE_LDFLAGS="-L$lib"
            fi
            QORE_LDFLAGS="$QORE_LDFLAGS -lqore"
            AC_MSG_RESULT([$lib (shared)])
        elif test "$mingw" = yes -a -f "$lib/libqore.${SHLIB_SUFFIX}.a"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                QORE_LDFLAGS="-L$lib"
            fi
            QORE_LDFLAGS="$QORE_LDFLAGS -lqore"
            AC_MSG_RESULT([$lib (shared)])
        elif test -f "$lib/libqore.a"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                QORE_LDFLAGS="-L$lib"
            fi
            QORE_LDFLAGS="$QORE_LDFLAGS -lqore"
            AC_MSG_RESULT([$lib (static)])
        fi
    fi
    if test -z "$QORE_LDFLAGS"; then
        return
    fi

    AC_MSG_CHECKING([for qore include files])
    # try to find include files
    if test "$a" = "/" -o -z "$a"; then
        inc=/usr/include
    else
        inc=$a/include
    fi
    if test -f "$inc/qore/Qore.h"; then
        AC_MSG_RESULT([found: $inc])
        set_qore_cppflags $inc
    else
        AC_MSG_ERROR([not found])
    fi

    AC_SUBST(QORE_LDFLAGS)
    AC_SUBST(QORE_CPPFLAGS)
}

# see if we can figure out where the qore library is
AC_ARG_WITH([qore-dir],
  [AS_HELP_STRING([--with-qore-dir@<:@=DIR@:>@],
                  [Qore directory])],
  [if test ! -d "${with_qore_dir}"; then AC_MSG_ERROR(bad value ${with_qore_dir} for --with-qore-dir); unset with_qore_dir; fi],
  [with_qore_dir="$QORE_DIR"])

AC_MSG_CHECKING([for qore libraries and header files])
for dir in "${with_qore_dir}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/qore /usr/local/qore /opt/local /sw /usr/sfw /opt/sfw; do
    find_qore $dir
    if test -n "$QORE_LDFLAGS"; then
        break
    fi
done
if test -z "$QORE_LDFLAGS"; then
    AC_MSG_ERROR([no qore library found])
else
   # test qore version
   AC_MSG_CHECKING([qore library version])

   # get qore version code
   qvc=`grep QORE_VERSION_CODE $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\  2>/dev/null`

   # get qore version string
   qore_version=`grep QORE_VERSION.\" $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\ |sed s/\"//g 2>/dev/null`

   # if version code not present, may be qore 0.7.0 or earlier
   if test -z "$qvc"; then
      qv_major=`grep QORE_VERSION_MAJOR $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\  2>/dev/null`
      if test -z "$qv_major"; then
         AC_MSG_ERROR([cannot determine; check Qore header files in $QORE_INC_DIR])
      fi

      qv_minor=`grep QORE_VERSION_MINOR $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\  2>/dev/null`
      qv_sub=`grep QORE_VERSION_SUB $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\  2>/dev/null`

      if test "$qv_major" -gt 0; then
         qvc=`printf "%d%02d%02d" $qv_major $qv_minor $qv_sub`
      else
         qvc=`printf "%d%02d" $qv_major $qv_minor $qv_sub`
      fi
      AC_DEFINE_UNQUOTED([QORE_VERSION_CODE], $qvc, [define when missing in Qore headers])
   fi

   if test $qvc -lt 20000; then
      AC_MSG_ERROR([found $qore_version; this module requires at least version 2.0])
   fi

   AC_MSG_RESULT([${qore_version} OK])

   MODULE_API=`qore --latest-module-api`
   AC_SUBST(MODULE_API)

   if test -z "${prefix}" -o "${prefix}" = "NONE"; then
      libdir=`qore --module-dir`
      userlibdir=`qore --user-module-dir 2>/dev/null`
      if test -z "${userlibdir}"; then
         userlibdir="${libdir}"
      fi
   else
      libdir="$prefix/lib${LIBSUFFIX}/qore-modules"
      qlibdir=`qore --module-dir`
      if test "$libdir" != "$qlibdir"; then
         AC_MSG_WARN([warning, installed qore's module dir is $qlibdir, this module will be installed in $libdir])
      fi
      if test -z "${datarootdir}"; then
         datarootdir="$prefix/share";
         AC_SUBST([datarootdir])
      fi
      if test -n "echo ${datarootdir}|grep \$"; then
         userlibdir="`eval echo ${datarootdir}`/qore-modules"
      else
         userlibdir="${datarootdir}/qore-modules"
      fi
      qlibdir=`qore --user-module-dir`
      if test "$userlibdir" != "$qlibdir"; then
         AC_MSG_WARN([warning, installed qore's user module dir is $qlibdir, this module will be installed in $userlibdir])
      fi
   fi
fi
AC_SUBST([userlibdir])
usermoddir="`echo ${userlibdir}|sed s/-modules.*//`"
AC_SUBST([usermoddir])

AC_ARG_ENABLE([profile],
     [AS_HELP_STRING([--enable-profile],
		     [turn on profiling support (default=no)])],
     [case "${enable_profile}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_profile} for --enable-profile) ;;
      esac],
     [enable_profile=no])

AC_ARG_ENABLE([debug],
     [AS_HELP_STRING([--enable-debug],
		     [turn on debugging (default=no)])],
     [case "${enable_debug}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_debug} for --enable-debug) ;;
      esac],
     [enable_debug=yes])

if test "${enable_debug}" = yes; then
   AC_DEFINE(DEBUG, 1, Define if debugging support should be included)
   # remove -O2 from CXXFLAGS so valgrind can report properly
   CXXFLAGS=`echo $CXXFLAGS | sed 's/\-O2//'`
else
   AC_DEFINE(NDEBUG, 1, Define if assert() declarations should be suppressed)
fi

if test "${enable_profile}" = yes; then
   AC_DEFINE(PROFILE, 1, Define if profiling support should be included)
fi

AC_ARG_ENABLE([optimization],
     [AS_HELP_STRING([--enable-optimization],
		     [turn on optimization (default=auto (yes unless debugging is enabled)])],
     [case "${enable_optimization}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_optimization} for --enable-optimization) ;;
      esac],
     [enable_optimization=auto])

if test "$enable_optimization" = "auto"; then
   if test "${enable_debug}" = yes; then
      enable_optimization=no
   else
      enable_optimization=yes
   fi
fi

# check for gcc visibility support
AC_MSG_CHECKING([for gcc visibility support])
if test "$GXX" = "yes"; then
   gcc_version=`$CXX -dumpversion`
   if test "$gcc_version" \> "4"; then
      AC_DEFINE(HAVE_GCC_VISIBILITY, 1, if compiling with g++ and visibility support is available)
      AC_MSG_RESULT([yes, $CXX $gcc_version])
      # we no longer set visibility to hidden by default because it makes RTTI symbols invisible
      # all functions in the library are tagged anyway
      #CXXFLAGS="$CXXFLAGS -fvisibility=hidden"
   else
      AC_MSG_RESULT([no, gcc $gcc_version])
   fi
else
   AC_MSG_RESULT([no, $CXX])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

AC_CHECK_HEADERS([stdint.h sys/select.h poll.h pwd.h])

# turn on optimizations if we know how
if test "$enable_optimization" = "yes"; then
    if test "$GXX" = "yes"; then
        CXXFLAGS="$CXXFLAGS -O3"  # gcc
    elif test "$CXX" = "CC" -a "`echo $host_os|cut -b-7`" = "solaris"; then
        # remove -g with Solaris CC as it reduces optimization levels
	CXXFLAGS="`echo $CXXFLAGS| sed s/-g//` -xO5" # Solaris CC
    elif test -n "`echo $CXX|grep aCC`"; then
        # remove -g with HP-UX aCC as it reduces optimization levels
	CXXFLAGS="`echo $CXXFLAGS| sed s/-g//` +O4" # HP-UX aCC
    fi
else
    if test "$GXX" = "yes"; then
        CXXFLAGS="`echo $CXXFLAGS| sed s/-O2//`"  # gcc
    fi
fi

# turn on some build flags for non-debugging, non-profiling builds
#if test "$enable_debug" = "no" -a "$enable_profile" = "no" -a "$enable_optimization" = "yes" ; then
    # we can no longer strip binaries and libraries with non-debugging, non-profiling builds on Darwin with -Wl,-x because it breaks linking :(
#fi

# turn on warnings if we know how
if test "$GXX" = "yes"; then
   CXXFLAGS="$CXXFLAGS -Wall"  # gcc
else
   if test "$solaris" = yes; then
      # turn on all warnings for the oracle sun studio compiler
      CXXFLAGS="$CXXFLAGS +w"
   fi
fi

AC_SUBST(OTHER_LIBS)

# save more version information in config.h
MODULE_VERSION_MAJOR=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f1 -d.`
MODULE_VERSION_MINOR=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f2 -d.`
MODULE_VERSION_SUB=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f3 -d.`

# set user module directory
mymodverdir=${qore_version}
AC_SUBST([mymodverdir])

# set version information
AC_DEFINE_UNQUOTED([MODULE_VERSION_MAJOR], $MODULE_VERSION_MAJOR, [major version number])
AC_DEFINE_UNQUOTED([MODULE_VERSION_MINOR], $MODULE_VERSION_MINOR, [minor version number])
AC_DEFINE_UNQUOTED([MODULE_VERSION_SUB], $MODULE_VERSION_SUB, [sub version number])
AC_DEFINE_UNQUOTED([MODULE_TARGET_ARCH], "$ARCH", [host type])
AC_DEFINE_UNQUOTED([MODULE_TARGET_OS], "$OS", [host type])
AC_DEFINE_UNQUOTED([MODULE_TARGET_BITS], $bits, [32 or 64 bit build])

AM_CONDITIONAL([COND_DOXYGEN],    [test -n "$with_doxygen"])
AM_CONDITIONAL([COND_DEBUG],      [test "$enable_debug" = yes])
AM_CONDITIONAL([COND_PROFILE],    [test "$enable_profile" = yes])
AM_CONDITIONAL([COND_SINGLE_COMPILATION_UNIT], [test "$enable_single_compilation_unit" = yes])
AM_CONDITIONAL([COND_MINGW],      [test "$mingw" = yes])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

echo "*** MODULE FEATURES ***"
printf "%-24s: %s %s" module "$OS" "$cpu_family"
if test "$enable_64bit" = "yes"; then
   printf " (64-bit)\n"
else
   printf " (32-bit)\n"
fi

show_library_feature() {
   printf "%-24s: " "$1"
   if test "$2" = "yes"; then
      printf "%-8s" yes
   else
      printf disabled
   fi
   if test -n "$3"; then
      printf " (%s)" $3
   fi
   echo
}

show_library_feature optimizations $enable_optimization
show_library_feature debug $enable_debug
show_library_feature profiling $enable_profile

# hack to use our libtool if on mingw to enable static libs to be linked in to the module
if test "$mingw" = yes; then
   # backup generated libtool
   if test ! -e libtool.orig ; then cp libtool libtool.orig; fi
   sed -e 's/deplibs_check_method=.*/deplibs_check_method=pass_all/g' -e 's/allow_undefined_flag=.*/allow_undefined_flag=yes/g' libtool > libtool.tmp
   mv libtool.tmp libtool
fi

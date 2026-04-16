dnl Process this file with autoconf to produce a configure script.

AC_INIT([talculator], [2.1.5devel])
AC_CONFIG_SRCDIR([configure.ac])
AM_INIT_AUTOMAKE

dnl AC_INIT(configure.ac)
dnl AM_INIT_AUTOMAKE([talculator], [2.1.5devel])
AM_MAINTAINER_MODE

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_CXX
IT_PROG_INTLTOOL([], [no-xml])

dnl
dnl libm
dnl 

LT_LIB_M

dnl
dnl GTK2 or GTK 3
dnl 

AC_ARG_ENABLE([gtk3],
    [AS_HELP_STRING([--enable-gtk3],
        [compile talculator against gtk+ 3.0 (default: yes)])],
    [enable_gtk3="${enableval}"],
    [enable_gtk3="yes"]
)

if test x"$enable_gtk3" = x"yes"; then
    pkg_modules="gtk+-3.0"
else
    pkg_modules="gtk+-2.0 >= 2.16.0"
fi

PKG_CHECK_MODULES(GTK, [$pkg_modules])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES(LIBQALCULATE, [libqalculate >= 5.0])
AC_SUBST(LIBQALCULATE_CFLAGS)
AC_SUBST(LIBQALCULATE_LIBS)

AM_PROG_CC_C_O

AC_DEFINE_UNQUOTED([PACKAGE], ["$PACKAGE_TARNAME"], [Name of package])
AC_DEFINE_UNQUOTED([VERSION], ["$PACKAGE_VERSION"], [Version number of package])

GETTEXT_PACKAGE=talculator
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of gettext package])
AC_SUBST(GETTEXT_PACKAGE)

LT_INIT

dnl Add the languages which your application supports here.
ALL_LINGUAS="am ar be bg cs da de el en_AU en_GB es_MX es et eu fi fr gl hr hu id it ja kk ko lt mr ms nl pl pt_BR pt ro ru sk sl sq sr sv tr uk zh_CN zh_TW"
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.22])

dnl If we don't have msgfmt, then po/ is going to fail -- ensure that
dnl AM_GNU_GETTEXT found it (this is from the libpurple project)

if test x$MSGFMT = xno
then
        AC_MSG_ERROR([

The msgfmt command is required to build talculator.  If it is installed
on your system, ensure that it is in your path.  If it is not, install
GNU gettext to continue.
])
fi

dnl Use -Wall if we have gcc.
changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac
fi
changequote([,])dnl

dnl m4/Makefile
AC_CONFIG_FILES([
Makefile
talculator.spec
src/Makefile
po/Makefile.in
ui/Makefile
ui/about.ui
doc/Makefile
pixmaps/Makefile
])
AC_OUTPUT

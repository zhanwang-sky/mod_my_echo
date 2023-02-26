include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_my_echo

mod_LTLIBRARIES = mod_my_echo.la
mod_my_echo_la_SOURCES  = mod_my_echo.c
mod_my_echo_la_CFLAGS   = $(AM_CFLAGS)
mod_my_echo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_my_echo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

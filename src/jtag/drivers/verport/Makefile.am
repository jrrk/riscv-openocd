include $(top_srcdir)/common.mk

AM_CXXFLAGS = -I/usr/local/share/verilator/include -I obj_dir -DVM_TRACE=1
AM_CFLAGS = -I$(top_srcdir)/src/jtag/drivers

noinst_LTLIBRARIES = libVglbl__ALL.la
libVglbl__ALL_la_SOURCES = sim_main.cpp verport.c

VERPORT_SRC = verport.c

noinst_HEADERS = Vtap_ext.h

MAINTAINERCLEANFILES = $(srcdir)/Makefile.in

AM_CPPFLAGS = \
	-I$(top_srcdir)/src

check_PROGRAMS = engine_accuracy_test
TESTS = $(check_PROGRAMS)

engine_accuracy_test_SOURCES = \
	engine_accuracy_test.c \
	$(top_srcdir)/src/engine.c \
	$(top_srcdir)/src/engine.h \
	$(top_srcdir)/src/engine_qalc_bridge.cpp \
	$(top_srcdir)/src/engine_qalc_bridge.h

engine_accuracy_test_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBQALCULATE_CFLAGS)

engine_accuracy_test_CXXFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBQALCULATE_CFLAGS)

engine_accuracy_test_LDADD = \
	$(GTK_LIBS) \
	$(LIBQALCULATE_LIBS) \
	$(LIBM)

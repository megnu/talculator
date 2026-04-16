## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/talculator/ui"\" \
	-DPACKAGE_PIXMAPS_DIR=\""$(datadir)/pixmaps"\" \
	-DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DGSEAL_ENABLE

bin_PROGRAMS = talculator

talculator_SOURCES = \
	main.c talculator.h \
	engine.c engine.h \
	engine_qalc_bridge.cpp engine_qalc_bridge.h \
	display.c display.h \
	general_functions.c general_functions.h \
	calc_basic.c calc_basic.h \
	config_file.c config_file.h \
	callbacks.c callbacks.h \
	ui.c ui.h

talculator_CFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBQALCULATE_CFLAGS)

talculator_CXXFLAGS = \
	$(GTK_CFLAGS) \
	$(LIBQALCULATE_CFLAGS)

talculator_LDADD = \
	$(GTK_LIBS) \
	$(LIBQALCULATE_LIBS) \
	$(LIBM)

talculator_LDFLAGS = -export-dynamic

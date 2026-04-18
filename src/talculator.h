/*
 *  talculator.h - general definitions.
 *	part of talculator
 *  	(c) 2002-2014 Simon Flöry (simon.floery@rechenraum.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _GALCULATOR_H
#define _GALCULATOR_H 1

#include <gtk/gtk.h>
#include <glib.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef WITH_HILDON
#include "hildon/hildon-program.h"
#include "hildon/hildon-window.h"
#include "glade/glade-build.h"
#endif

#define DEFAULT_DEC_POINT '.'

#define CLEARED_DISPLAY	"0"

/* old, non-XDG method */
#define CONFIG_FILE_NAME_OLD ".talculator"
/* XDG spec */
#define CONFIG_FILE_NAME "talculator.conf"

#ifdef WITH_HILDON
#define MAIN_GLADE_FILE 			PACKAGE_UI_DIR "/main_frame_hildon.ui"
#else
#define MAIN_GLADE_FILE 			PACKAGE_UI_DIR "/main_frame.ui"
#endif
#define CLASSIC_VIEW_GLADE_FILE		PACKAGE_UI_DIR "/classic_view.ui"
#define PAPER_VIEW_GLADE_FILE		PACKAGE_UI_DIR "/paper_view.ui"

#define SCIENTIFIC_GLADE_FILE		PACKAGE_UI_DIR "/scientific_buttons_gtk3.ui"
#define BASIC_GLADE_FILE			PACKAGE_UI_DIR "/basic_buttons_gtk3.ui"
#define DISPCTRL_RIGHT_GLADE_FILE	PACKAGE_UI_DIR "/dispctrl_right_gtk3.ui"
#define DISPCTRL_RIGHTV_GLADE_FILE	PACKAGE_UI_DIR "/dispctrl_right_vertical_gtk3.ui"
#define DISPCTRL_BOTTOM_GLADE_FILE	PACKAGE_UI_DIR "/dispctrl_bottom_gtk3.ui"

#define ABOUT_GLADE_FILE 			PACKAGE_UI_DIR "/about.ui"
#ifdef WITH_HILDON
#define PREFS_GLADE_FILE 			PACKAGE_UI_DIR "/prefs-ume.ui"
#else
#define PREFS_GLADE_FILE 			PACKAGE_UI_DIR "/prefs_gtk3.ui"
#endif

#define MY_INFINITY_STRING "inf"

/* i18n */

#include <libintl.h>
#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/* also change this in calc_basic.h */
#ifndef BUG_REPORT
	#define BUG_REPORT	_("Please submit a bugreport.")
#endif

/* if we do not get infinity from math.h, we try to define it by ourselves */
#include <math.h>
#ifndef INFINITY
	#define INFINITY 1.0 / 0.0
#endif

#ifndef PROG_NAME
	#define PROG_NAME	PACKAGE
#endif

/* CS_xxxx define flags for current_status. */

enum {
	CS_DEC,
	CS_HEX,
	CS_OCT,
	CS_BIN,
	NR_NUMBER_BASES
};

enum {
	CS_DEG,
	CS_RAD,
	CS_GRAD,
	NR_ANGLE_BASES
};

enum {
	CS_ALG,			/* algebraic notation */
	CS_RPN,			/* reverse polish notation */
	NR_NOTATION_MODES
};

enum {
	CS_FMOD_FLAG_INV,
	CS_FMOD_FLAG_HYP
};

enum {
	BASIC_MODE,
	SCIENTIFIC_MODE,
	PAPER_MODE,
	NR_MODES
};

enum {
	CONST_NAME_COLUMN,
	CONST_VALUE_COLUMN,
	CONST_DESC_COLUMN,
	NR_CONST_COLUMNS
};

enum {
	UFUNC_NAME_COLUMN,
	UFUNC_VARIABLE_COLUMN,
	UFUNC_EXPRESSION_COLUMN,
	NR_UFUNC_COLUMNS
};

enum {
	DISPCTRL_NONE,
	DISPCTRL_RIGHT,
	DISPCTRL_RIGHTV,
	DISPCTRL_BOTTOM,
	NR_DISPCTRL_LOCS
};

typedef struct {
	unsigned	number:2;
	unsigned	angle:2;
	unsigned	notation:2;
	unsigned	fmod:2;
	gboolean	calc_entry_start_new;
	gboolean	rpn_stack_lift_enabled;
	gboolean	allow_arith_op;
} s_current_status;

typedef struct {
	char		*button_name;
	/* for simplicity we put the display_names not in an array */
	char		*display_names[4];
} s_function_map;

typedef struct {
	char		*button_name;
	char 		*display_string;
	void		(*func)(GtkToggleButton *button);
} s_gfunc_map;

typedef struct {
	char		*button_name;
	/* display_string: what to display in history or formula entry. hasn't 
	 * to be the button label, e.g. "n!" and "!" 
	 */
	char 		*display_string;
	int		operation;
} s_operation_map;

typedef struct {
	int		x;
	int		y;
} s_point;

typedef struct {
	char		*desc;
	char		*name;
	char		*value;
} s_constant;

typedef struct {
	char 		*name;
	char 		*variable;
	char 		*expression;
} s_user_function;

typedef struct {
	char		**data;
	int		len;
} s_array;

#define TALC_MAX_SESSION_TABS 6

typedef struct s_preferences s_preferences;
extern s_preferences	prefs;
extern s_constant 	*constant;
extern s_user_function	*user_function;
#include "engine.h"
extern talc_engine	*calc_engine;

typedef struct {
	s_current_status	tab_current_status;
	int			tab_mode;
	gboolean		tab_vis_number;
	gboolean		tab_vis_angle;
	gboolean		tab_vis_notation;
	gboolean		tab_vis_funcs;
	gboolean		tab_vis_logic;
	gboolean		tab_vis_dispctrl;
	gboolean		tab_vis_standard;
	int			tab_def_number;
	int			tab_def_angle;
	s_array			tab_memory;
	GtkTextView		*tab_display_view;
	GtkTextBuffer		*tab_display_buffer;
	int			tab_display_result_counter;
	int			tab_display_result_line;
	char			tab_display_last_arith;
	int			tab_display_brackets;
	char			*tab_display_value;
	GtkBuilder		*tab_view_xml;
	GtkBuilder		*tab_button_box_xml;
	GtkBuilder		*tab_dispctrl_xml;
	GtkBuilder		*tab_classic_view_xml;
	GtkBuilder		*tab_paper_view_xml;
	char			**tab_rpn_stack;
	int			tab_rpn_stack_len;
} s_tab_context;

typedef struct {
	int			mode;
	int			number;
	int			angle;
	int			notation;
	char			*display_value;
	char			**rpn_stack;
	int			rpn_stack_len;
	char			**mem_values;
	int			memory_len;
} s_session_tab_state;

typedef struct {
	int			tab_count;
	int			active_tab;
	s_session_tab_state	tabs[TALC_MAX_SESSION_TABS];
} s_session_state;

extern s_tab_context *active_tab;

#define current_status (active_tab->tab_current_status)
#define memory (active_tab->tab_memory)
#define view_xml (active_tab->tab_view_xml)
#define button_box_xml (active_tab->tab_button_box_xml)
#define dispctrl_xml (active_tab->tab_dispctrl_xml)
#define classic_view_xml (active_tab->tab_classic_view_xml)
#define paper_view_xml (active_tab->tab_paper_view_xml)

#endif /* talculator.h */

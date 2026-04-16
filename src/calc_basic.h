/*
 *  calc_basic.h - arithmetic precedence handling and computing in basic 
 *			calculator mode.
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
 
#ifndef _CALC_BASIC_H
#define _CALC_BASIC_H 1

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef PROG_NAME
	#define PROG_NAME	PACKAGE
#endif

#ifndef BUG_REPORT
	#define BUG_REPORT	"Please submit a bugreport."
#endif

#define RPN_FINITE_STACK		3
#define RPN_INFINITE_STACK		-1

#include <glib.h>

void rpn_init (int size, int debug_level);
void rpn_stack_set_array (char **values, int length);
int rpn_stack_length (void);
void rpn_stack_push (const char *number);
char *rpn_stack_operation (char operation, const char *number);
char *rpn_stack_rolldown (const char *x);
char *rpn_stack_swapxy (const char *x);
char **rpn_stack_get (int length);
void rpn_stack_set_size (int size);
void rpn_free ();

#endif /* calc_basic.h */

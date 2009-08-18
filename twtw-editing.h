/*
 *  twtw-editing.h
 *  TwentyTwenty
 *
 *  Created by Pauli Ojala on 29.11.2008.
 *  Copyright 2008 Pauli Olavi Ojala. All rights reserved.
 *
 */
/*
    This file is part of TwentyTwenty.

    TwentyTwenty is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    TwentyTwenty is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TwentyTwenty.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _TWTW_EDITING_H_
#define _TWTW_EDITING_H_

#include "twtw-curves.h"


#ifdef __cplusplus
extern "C" {
#endif

/*
  These functions handle the creation of a TwentyTwenty curvelist from mouseDown/mouseDragged/mouseUp events
*/

TwtwCurveList *twtw_editing_curvelist_create_with_start_cursor_point (TwtwPoint point);

void twtw_editing_curvelist_add_cursor_point (TwtwCurveList *curvelist, TwtwPoint point);

void twtw_editing_curvelist_finish_at_cursor_point (TwtwCurveList *curvelist, TwtwPoint point);


/*
  Undo support
*/

typedef enum {
    TWTW_NO_ACTION = 0,
    
    TWTW_ACTION_DELETE_LAST_CURVE = 10,
    TWTW_ACTION_SET_CURVES = 50,
    TWTW_ACTION_SET_PCM_SOUND = 100,
    TWTW_ACTION_SET_BG_PHOTO = 200,
    
    TWTW_ACTION_SET_TITLE = 500,
    TWTW_ACTION_SET_AUTHOR,
    TWTW_ACTION_SET_DOCFLAGS
} TwtwActionType;

typedef void (*TwtwActionDestructorFuncPtr)(void *);

// destructorFunc gets called when the action is removed from the undo stack.
// it must free 'data' (and in the case of e.g. PCM sound files, it should also remove the temp file).
// if destructorFunc is NULL, data will be destroyed with g_free().
typedef struct _TwtwAction {
    gint type;
    
    gint targetPageIndex;    // pass -1 if the action doesn't target a specific page
    void *targetSpecifier;   // reserved for expansion
    
    void *data;
    TwtwActionDestructorFuncPtr destructorFunc;
} TwtwAction;

 
void twtw_undo_push_action (TwtwAction *action);

gint twtw_undo_pop_and_perform ();

gint twtw_undo_get_topmost_action_type ();

gint twtw_undo_get_stack_count ();

void twtw_undo_clear_stack ();


// this is needed for the SET_CURVES action


#ifdef __cplusplus
}
#endif

#endif


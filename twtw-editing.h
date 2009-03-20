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


/*
These functions handle the creation of a TwentyTwenty curvelist from mouseDown/mouseDragged/mouseUp events
*/

TwtwCurveList *twtw_editing_curvelist_create_with_start_cursor_point (TwtwPoint point);

void twtw_editing_curvelist_add_cursor_point (TwtwCurveList *curvelist, TwtwPoint point);

void twtw_editing_curvelist_finish_at_cursor_point (TwtwCurveList *curvelist, TwtwPoint point);

#endif

#ifndef _TERMINAL_FIND_H_
#define _TERMINAL_FIND_H_

/*
 * Copyright © 2009 Richard Russon (flatcap)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

#define TERMINAL_FIND_FLAG_CASE   (1 << 0)
#define TERMINAL_FIND_FLAG_REGEX  (1 << 1)
#define TERMINAL_FIND_FLAG_WHOLE  (1 << 2)

typedef struct
{
  char *find_string;
  char *regex_string;
  int   row;
  int   column;
  int   length;
  int   flags;
  void *screen;
} FindParams;

void terminal_find_display (GtkWindow *parent);

#endif /* _TERMINAL_FIND_H_ */


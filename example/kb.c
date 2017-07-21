/* Build me with:
 gcc -shared -o kb.so -undefined dynamic_lookup -fPIC kb.c ../src/*.c -lncurses
 gcc -shared -o kb.so -undefined dynamic_lookup kb.c -lncurses
 gcc -O2 -fpic -c -o kb.o kb.c -lncurses
 gcc -O -shared -fpic -o kb.so kb.o -lncurses


 works: gcc -shared -o kb.so -undefined dynamic_lookup -fPIC kb.c ../src/*.c -lncurses

*/

/* Copyright (C) 2012 Ross Andrews
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/lgpl.txt>. */

#include <ncurses.h>
#include "../src/aria.h"
#define AR_DL_DLOPEN

ar_Value *getch_wrapper(ar_State *S, ar_Value* args);

int ar_open_kb(ar_State *S){
  struct { const char *name; ar_CFunc fn; } funcs[] = {
    {"getch", getch_wrapper},
    {NULL, NULL}
  };
  initscr(); /* Start curses */
  raw(); /* Turn off line buffering */
  set_escdelay(25); /* Shorten delay after ESC key to something reasonable */
  keypad(stdscr, TRUE); /* Grab ALL kbd input */
  refresh(); /* Store screen state so endwin works */
  endwin(); /* Leave curses mode */
  for (int i = 0; funcs[i].name; i++) {
    ar_bind_global(S, funcs[i].name, ar_new_cfunc(S, funcs[i].fn));
  }

  return 0;
}

ar_Value *getch_wrapper(ar_State *S, ar_Value* args){
  reset_prog_mode(); /* Get back into curses */
  ar_Value *res = ar_new_number(S, getch()); /* Grab a char and push it */
  endwin(); /* Get out of curses again */
  return res;
}

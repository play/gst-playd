/* 
   gst_playd - GStreamer backend for Play

   Copyright (C) 2012 Paul Betts

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/

#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>

#define EXIT_FAILURE 1

/* The name the program was run with, stripped of any leading path. */
char *program_name;

int
main (int argc, char **argv)
{
	printf("Foo!\n");
	exit(0);
}

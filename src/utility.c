/*
   utility.c - Misc helper functions

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

#include <glib.h>
#include <string.h>
#include <sys/uio.h>
#include <zmq.h>

#include "utility.h"

gboolean util_close_socket(void* sock)
{
	if (!sock) return TRUE;

	if (zmq_close(sock) == -1) {
		g_warning("Failed to close socket: %s", zmq_strerror(zmq_errno()));
		return FALSE;
	}

	return TRUE;
}

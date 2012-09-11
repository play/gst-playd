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

#include <glib.h>
#include <glib/goption.h>

#define EXIT_FAILURE 1

static gboolean verbose = FALSE;
static char* client_message = NULL;

static GOptionEntry entries[] = {
	 { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
	 { "send-message", 's', 0, G_OPTION_ARG_STRING, &client_message, "Send a message to a running gst_playd and exit", NULL },
};

int main (int argc, char **argv)
{
	GError* err = NULL;
	GOptionContext* ctx;

	ctx = g_option_context_new(" - A GStreamer backend daemon for Play");
	g_option_context_add_main_entries(ctx, entries, "");
	if (!g_option_context_parse(ctx, &argc, &argv, &err)) {
		g_error("Option parsing failed: %s", err->message);
		return 1;
	}

	g_option_context_free(ctx);
	return 0;
}

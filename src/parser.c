/*
   parser.c - Message parsing and dispatch

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

#include "parser.h"
#include "operations.h"

struct message_dispatch {
	const char* prefix;
	char* (*func)(const char*);
};

static struct message_dispatch messages[] = {
	{ "PING", op_ping, },
	{ NULL },
};

char* parse_message(const char* message)
{
	char* ret = strdup("FAIL Message is Invalid");

	GError* err = NULL;
	GRegex* msg_regex = g_regex_new("^([A-Z]+) (.+)$", 0, 0, &err);
	char* prefix = NULL;

	GMatchInfo* match_info = NULL;
	if (!g_regex_match(msg_regex, message, 0, &match_info)) {
		g_warning("Message is invalid: %s", message);
		goto out;
	}

	prefix = g_match_info_fetch(match_info, 1);
	if (!prefix) {
		goto out;
	}

	struct message_dispatch* cur = messages;
	while (cur->prefix) {
		char* param = NULL;
		if (strcmp(cur->prefix, prefix)) continue;

		param = g_match_info_fetch(match_info, 2);

		g_free(ret);
		ret = (*(cur->func))(param);
		g_free(param);
		break;
	}

out:
	if (prefix) g_free(prefix);
	if (match_info) g_match_info_free(match_info);
	g_regex_unref(msg_regex);
	return ret;
}

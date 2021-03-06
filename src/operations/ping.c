/*
   ping.h - Ping operations

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
#include "ping.h"
#include "utility.h"
#include "op_services.h"

#include "operations/ping.h"

static struct message_dispatch_entry ping_messages[] = {
	{ "PING", op_ping_parse },
	{ NULL },
};

void* op_ping_new(void* services)
{
	return services;
}

gboolean op_ping_register(void* ctx, struct message_dispatch_entry** entries)
{
	*entries = ping_messages;
	return TRUE;
}

void op_ping_free(void* dontcare)
{
}

char* op_ping_parse(const char* param, void* ctx)
{
	struct op_services* services = (struct op_services*)ctx;

	if (!param) param = "(none)";
	char* ret = g_strdup_printf("OK Message was %s", param);

	pubsub_send_message(services->pub_sub, ret);
	return ret;
}

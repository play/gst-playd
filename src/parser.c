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
	void* (*op_new)();
	char* (*op_parse)(const char*, void*);
	void (*op_free)(void* ctx);
};

struct parse_ctx {
	GHashTable* ctx_table;
};

static struct message_dispatch messages[] = {
	{ "PING", op_ping_new, op_ping_parse, op_ping_free, },
	{ NULL },
};

struct parse_ctx* parse_new(void)
{
	struct parse_ctx* ret = g_new0(struct parse_ctx, 1);

	ret->ctx_table = g_hash_table_new(g_str_hash, g_str_equal);

	struct message_dispatch* cur = messages;
	do {
		void* ctx = (*cur->op_new)();
		g_hash_table_insert(ret->ctx_table, (gpointer)cur->prefix, ctx);
	} while ((++cur)->prefix);

	return ret;
}

static void free_ctx_func(gpointer key, gpointer value, gpointer user_data)
{
	struct message_dispatch* cur = messages;
	do {
		if (strcmp(cur->prefix, key)) continue;

		(*cur->op_free)(value);
		break;
	} while ((++cur)->prefix);
}

void parse_free(struct parse_ctx* ctx)
{
	g_hash_table_foreach(ctx->ctx_table, free_ctx_func, NULL);
	g_hash_table_destroy(ctx->ctx_table);
	g_free(ctx);
}

char* parse_message(struct parse_ctx* ctx, const char* message)
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
	do {
		char* param = NULL;
		if (strcmp(cur->prefix, prefix)) continue;

		param = g_match_info_fetch(match_info, 2);

		void* op_ctx = g_hash_table_lookup(ctx->ctx_table, prefix);

		g_free(ret);
		ret = (*cur->op_parse)(param, op_ctx);
		g_free(param);
		break;
	} while ((++cur)->prefix);

out:
	if (prefix) g_free(prefix);
	if (match_info) g_match_info_free(match_info);
	g_regex_unref(msg_regex);
	return ret;
}

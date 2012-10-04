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

struct reg_entry_with_ctx {
	const char* prefix;
	void* plugin_context;

	parse_handler_cb parser;
};

struct plugin_entry_with_ctx {
	void *ctx;
	void (*plugin_free)(void* ctx);
};

struct parse_ctx {
	GHashTable* message_table; 	/* prefix -> reg_entry_with_ctx */
	GSList* plugin_list; 		/* list of plugin_entry_with_ctx */
};

static void plugin_entry_free(void* entry);

struct parse_ctx* parse_new(void)
{
	struct parse_ctx* ret = g_new0(struct parse_ctx, 1);

	ret->plugin_list = NULL;
	ret->message_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	return ret;
}

void parse_free(struct parse_ctx* parser)
{
	g_hash_table_destroy(parser->message_table);
	g_slist_free_full(parser->plugin_list, plugin_entry_free);

	g_free(parser);
}

gboolean parse_register_plugin(struct parse_ctx* parser, struct parser_plugin_entry* plugin)
{
	void* plugin_ctx = (*plugin->plugin_new)(plugin->context);

	struct message_dispatch_entry* regd_messages = NULL;
	struct plugin_entry_with_ctx* p_entry;
	struct reg_entry_with_ctx* r_entry;
	char* prefix;

	if (!(*plugin->plugin_register)(plugin_ctx, &regd_messages)) {
		g_warning("plugin failed to register: %s", plugin->friendly_name);
		(*plugin->plugin_free)(plugin_ctx);

		return FALSE;
	}

	p_entry = g_new0(struct plugin_entry_with_ctx, 1);
	p_entry->ctx = plugin_ctx;
	p_entry->plugin_free = plugin->plugin_free;
	parser->plugin_list = g_slist_prepend(parser->plugin_list, p_entry);

	for (struct message_dispatch_entry* msg = regd_messages; msg->prefix; msg++) {
		r_entry = g_new0(struct reg_entry_with_ctx, 1);
		prefix = strdup(msg->prefix);

		r_entry->prefix = prefix;
		r_entry->plugin_context = plugin_ctx;
		r_entry->parser = msg->op_parse;

		g_hash_table_insert(parser->message_table, prefix, r_entry);
	}

	g_print("Registered plugin: %s", plugin->friendly_name);
	return TRUE;
}

char* parse_message(struct parse_ctx* parser, const char* message)
{
	char* ret = strdup("FAIL Message is Invalid");

	GError* err = NULL;
	GRegex* msg_regex = g_regex_new("^([A-Z]+) (.+)$", 0, 0, &err);
	char* prefix = NULL;
	char* param;
	struct reg_entry_with_ctx* prefix_entry;

	GMatchInfo* match_info = NULL;
	if (!g_regex_match(msg_regex, message, 0, &match_info)) {
		g_warning("Message is invalid: %s", message);
		goto out;
	}

	prefix = g_match_info_fetch(match_info, 1);
	if (!prefix) {
		goto out;
	}

	prefix_entry = g_hash_table_lookup(parser->message_table, prefix);
	if (!prefix_entry) {
		g_warning("Message is invalid: %s", message);
		goto out;
	}

	param = g_match_info_fetch(match_info, 2);
	g_free(ret); /* free the error message */
	ret = (*prefix_entry->parser)(param, prefix_entry->plugin_context);
	g_free(param);

out:
	if (prefix) g_free(prefix);
	if (match_info) g_match_info_free(match_info);
	g_regex_unref(msg_regex);

	return ret;
}

static void plugin_entry_free(void* entry)
{
	struct plugin_entry_with_ctx* e = (struct plugin_entry_with_ctx*)entry;
	(*e->plugin_free)(e->ctx);
	g_free(entry);
}

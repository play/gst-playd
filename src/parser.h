/*
   parser.h - Message parsing and dispatch

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

#ifndef _PARSER_H
#define _PARSER_H

struct parse_ctx;

typedef char* (*parse_handler_cb) (const char* prefix, void* ctx);

struct message_dispatch_entry {
	const char* prefix;
	parse_handler_cb op_parse;
};

struct parser_plugin_entry {
	const char* friendly_name;

	void* (*plugin_new)(void);
	gboolean (*plugin_register)(void* ctx, struct message_dispatch_entry** entries);
	void (*plugin_free)(void* ctx);
};

struct parse_ctx* parse_new();
void parse_free(struct parse_ctx* parser);
gboolean parse_register_plugin(struct parse_ctx* parser, struct parser_plugin_entry* plugin);
char* parse_message(struct parse_ctx* parser, const char* message);

#endif

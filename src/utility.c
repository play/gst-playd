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

#include <stdio.h>
#include <glib.h>
#include <string.h>
#include <sys/uio.h>
#include <zmq.h>

#include "utility.h"


void util_zmq_glib_free(void* to_free, void* hint)
{
	g_free(to_free);
}

gboolean util_close_socket(void* sock)
{
	if (!sock) return TRUE;

	if (zmq_close(sock) == -1) {
		g_warning("Failed to close socket: %s", zmq_strerror(zmq_errno()));
		return FALSE;
	}

	return TRUE;
}

char* util_send_reqrep_msg(void* zmq_context, const char* message, const char* address)
{
	char* ret = NULL;
	int linger = 15*1000;
	void* sock = zmq_socket(zmq_context, ZMQ_REQ);

	if (!sock) {
		g_warning("Failed to create socket: %s", zmq_strerror(zmq_errno()));
		return ret;
	}

	zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(int));

	g_print("Connecting to %s\n", address);
	if (zmq_connect(sock, address) == -1) {
		g_warning("Failed to connect: %s", zmq_strerror(zmq_errno()));
		goto out;
	}

	zmq_msg_t msg;
	zmq_msg_init_data(&msg, (void*) message, sizeof(char) * strlen(message), NULL, NULL);
	zmq_msg_send(&msg, sock, 0);
	zmq_msg_close(&msg);

	zmq_msg_t rep_msg;
	zmq_msg_init(&rep_msg);
	zmq_msg_recv(&rep_msg, sock, 0);

	char* rep_text = g_new0(char, zmq_msg_size(&rep_msg) + 1);
	memcpy(rep_text, zmq_msg_data(&rep_msg), zmq_msg_size(&rep_msg));
	zmq_msg_close(&rep_msg);

	ret = rep_text;

out:
	util_close_socket(sock);
	return ret;
}

static void hash_foreach_calc_length(gpointer key, gpointer value, gpointer user_data)
{
	int* len = (int*)user_data;
	*len = (*len) + strlen((char*)key) + strlen((char*)value) + 2; /*newlines*/
}

static void hash_foreach_create_message(gpointer key, gpointer value, gpointer user_data)
{
	/* XXX: I hate this code and it makes me want to die */
	char** data = (char**)user_data;
	*data += sprintf(*data, "%s\n%s\n", (char*)key, (char*)value);
}

char* util_hash_table_as_string(GHashTable* table)
{
	char* ret;
	int bufsize = 4;

	g_hash_table_foreach(table, hash_foreach_calc_length, &bufsize);

	ret = g_new0(char, bufsize);

	char* iter = ret;
	g_hash_table_foreach(table, hash_foreach_create_message, &iter);

	return ret;
}


static void hash_foreach_stringify_table(gpointer key, gpointer value, gpointer user_data)
{
	int int_key = *((int*)key);
	GHashTable* table = (GHashTable*)user_data;

	g_hash_table_insert(table, g_strdup_printf("%d", int_key), g_strdup((char*)value));
}

char* util_int_hash_table_as_string(GHashTable* table)
{
	GHashTable* temp_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	g_hash_table_foreach(table, hash_foreach_stringify_table, temp_table);

	char* ret = util_hash_table_as_string(temp_table);
	g_hash_table_destroy(temp_table);

	return ret;
}

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
#include <zmq.h>

#define EXIT_FAILURE 1

static gboolean verbose = FALSE;
static char* client_message = NULL;
static int icecast_port = 8000;

static GOptionEntry entries[] = {
	 { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
	 { "send-message", 's', 0, G_OPTION_ARG_STRING, &client_message, "Send a message to a running gst_playd and exit", NULL },
	 { "port", 'p', 0, G_OPTION_ARG_INT, &icecast_port, "Set the port that Icecast will bind to", NULL },
	 { NULL },
};

struct timer_closure {
	void* zmq_context;
	char* address;
};

static char* zeromq_address_from_port(int port)
{
	return g_strdup_printf("tcp://*:%d", port + 10000);
}

static gboolean send_client_message(void* zmq_context, const char* message, const char* address)
{
	gboolean ret = TRUE;
	int linger = 5*1000;
	void* sock = zmq_socket(zmq_context, ZMQ_REQ);
	zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(int));

	g_warning("Connecting to %s", address);
	if (!zmq_connect(sock, address)) {
		g_warning("Failed to connect: %s", zmq_strerror(zmq_errno()));
		ret = FALSE; goto out;
	}

	zmq_msg_t msg;
	zmq_msg_init_data(&msg, message, sizeof(char) * strlen(message), NULL, NULL);
	zmq_send(sock, &msg, ZMQ_NOBLOCK);

out:
	if (sock) {
		if (!zmq_close(sock)) {
			g_warning("Failed to close socket: %s", zmq_strerror(zmq_errno()));
			ret = FALSE;
		}
	}

	return ret;
}

static gboolean handle_incoming_messages(gpointer user_data)
{
	g_warning("Tick");
	return TRUE;
}

int main (int argc, char **argv)
{
	int ret = 0;

	GError* err = NULL;
	GOptionContext* ctx;

	void* zmq_ctx = NULL;

	ctx = g_option_context_new(" - A GStreamer backend daemon for Play");
	g_option_context_add_main_entries(ctx, entries, "");

	if (!g_option_context_parse(ctx, &argc, &argv, &err)) {
		g_error("Option parsing failed: %s", err->message);
		ret = EXIT_FAILURE;
		goto out;
	}

	zmq_ctx = zmq_init(1);

	if (client_message) {
		char* address = zeromq_address_from_port(icecast_port);
		ret = send_client_message(zmq_ctx, client_message, address) ? 0 : 1;
		g_free(address);
		goto out;
	}

	/*
	 * Server Mainloop
	 */

	GMainLoop* main_loop = g_main_loop_new(NULL, FALSE);

	struct timer_closure closure = { zmq_ctx, zeromq_address_from_port(icecast_port) };
	g_timeout_add(250, handle_incoming_messages, &closure);

	g_unix_signal_add(SIGINT, g_main_loop_quit, main_loop);

	g_warning("Starting Main Loop");
	g_main_loop_run(main_loop);
	g_warning("Bailing");

	g_free(closure.address);

out:
	if (zmq_ctx) zmq_term(zmq_ctx);

	g_option_context_free(ctx);
	return ret;
}

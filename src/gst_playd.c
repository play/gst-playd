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
#include <sys/uio.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <zmq.h>

#ifdef G_OS_UNIX
#include <glib-unix.h>
#endif

#include "parser.h"
#include "operations.h"
#include "utility.h"
#include "op_services.h"

#define EXIT_FAILURE 1

static gboolean verbose = FALSE;
static gboolean pubsub_listen = FALSE;
static char* client_message = NULL;
static int icecast_port = 8000;

static GOptionEntry entries[] = {
	 { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
	 { "send-message", 's', 0, G_OPTION_ARG_STRING, &client_message, "Send a message to a running gst_playd and exit", NULL },
	 { "events-listen", 'e', 0, G_OPTION_ARG_NONE, &pubsub_listen, "Listen to the event stream of a running gst_playd (for debugging purposes)", NULL },
	 { "port", 'p', 0, G_OPTION_ARG_INT, &icecast_port, "Set the port that Icecast will bind to", NULL },
	 { NULL },
};

struct timer_closure {
	void* zmq_socket;
	struct parse_ctx* parse_ctx;
	GMainLoop* main_loop;
	gboolean should_quit;
	gboolean pubsub_mode;
};

static struct parser_plugin_entry parser_operations[] = {
	{ "Ping", NULL, op_ping_new, op_ping_register, op_ping_free },
	{ "Control", NULL, op_control_new, op_control_register, op_control_free },
	{ NULL },
};

static char* zeromq_address_from_port(const char* address, int port)
{
	return g_strdup_printf("tcp://%s:%d", address, port + 10000);
}

static int handle_message(void* zmq_sock, struct parse_ctx* parser)
{
	int ret = 0;
	zmq_msg_t msg;
	char* message_text = NULL;

	zmq_msg_init(&msg);
	if (zmq_msg_recv(&msg, zmq_sock, ZMQ_DONTWAIT) == -1) {
		switch (ret = zmq_errno()) {
		case EAGAIN:
			goto out;
		case EINTR:
			/* We'll pretend we "succeeded" so we'll look for a new message */
			ret = 0;
			goto out;
		default:
			g_warning("Failed to recieve message: 0x%x (%s)", ret, zmq_strerror(ret));
			goto out;
		}
	}

	message_text = g_new0(char, zmq_msg_size(&msg) + 1);
	memcpy(message_text, zmq_msg_data(&msg), zmq_msg_size(&msg));

	char* data = parse_message(parser, message_text);
	g_warning("About to send reply: %s", data);

	zmq_msg_t rep_msg;
	zmq_msg_init_data(&rep_msg, (void*)data, sizeof(char) * strlen(data), util_zmq_glib_free, NULL);
	zmq_msg_send(&rep_msg, zmq_sock, 0);
	zmq_msg_close(&rep_msg);

out:
	if (message_text) g_free(message_text);
	zmq_msg_close(&msg);
	return ret;
}

static int handle_pubsub_message(void* zmq_sock)
{
	int ret = 0;
	zmq_msg_t msg;
	char* message_text = NULL;

	zmq_msg_init(&msg);
	if (zmq_msg_recv(&msg, zmq_sock, ZMQ_DONTWAIT) == -1) {
		switch (ret = zmq_errno()) {
		case EAGAIN:
			goto out;
		case EINTR:
			/* We'll pretend we "succeeded" so we'll look for a new message */
			ret = 0;
			goto out;
		default:
			g_warning("Failed to recieve message: 0x%x (%s)", ret, zmq_strerror(ret));
			goto out;
		}
	}

	message_text = g_new0(char, zmq_msg_size(&msg) + 1);
	memcpy(message_text, zmq_msg_data(&msg), zmq_msg_size(&msg));

	g_print("%s\n", message_text);

out:
	if (message_text) g_free(message_text);
	zmq_msg_close(&msg);
	return ret;
}

static gboolean handle_incoming_messages(gpointer user_data)
{
	struct timer_closure* closure = (struct timer_closure*) user_data;

	if (closure->should_quit) {
		g_main_loop_quit(closure->main_loop);
		return FALSE;
	}

	if (closure->pubsub_mode) {
		while (handle_pubsub_message(closure->zmq_socket) == 0) {
			g_debug("Processing new message");
		}
	} else {
		while (handle_message(closure->zmq_socket, closure->parse_ctx) == 0) {
			g_debug("Processing new message");
		}
	}

	return TRUE;
}

static gboolean handle_sigint(void* shouldquit)
{
	gboolean* should_quit = shouldquit;
	*should_quit = TRUE;

	return TRUE;
}

static void* create_server_socket(void* zmq_ctx, int icecast_port)
{
	void* sock;
	void* ret = NULL;
	char* address = zeromq_address_from_port("127.0.0.1", icecast_port);

	int linger = 5*1000;
	sock = zmq_socket(zmq_ctx, ZMQ_REP);

	if (!sock) {
		g_warning("Failing to create socket %s", zmq_strerror(zmq_errno()));
		goto out;
	}

	zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(int));

	if (zmq_bind(sock, address) == -1) {
		g_warning("Failed to start server on address %s: %s", address, zmq_strerror(zmq_errno()));
		goto out;
	}

	ret = sock;

out:
	g_free(address);
	return ret;
}

static void* create_pubsub_socket(void* zmq_ctx, int icecast_port)
{
	char* repreq_addr = zeromq_address_from_port("127.0.0.1", icecast_port);
	char* pubsub_addr = NULL;
	void* ret = NULL;
	int linger = 5*1000;
	
	if (!(pubsub_addr = util_send_reqrep_msg(zmq_ctx, "PUBSUB ", repreq_addr))) {
		g_warning("Couldn't connect to %s to get PUB/SUB address.\nCheck to see if the server is down", repreq_addr);
		goto out;
	}

	if (pubsub_addr[0] != 'O' || pubsub_addr[1] != 'K') {
		g_warning("Invalid server response: %s. Maybe versions have changed?", pubsub_addr);
		goto out;
	}

	g_warning("Connecting to SUB %s", pubsub_addr+3);
	ret = zmq_socket(zmq_ctx, ZMQ_SUB);

	if (!ret) {
		g_warning("Failing to create socket %s", zmq_strerror(zmq_errno()));
		goto out;
	}

	zmq_setsockopt(ret, ZMQ_LINGER, &linger, sizeof(int));
	zmq_setsockopt(ret, ZMQ_SUBSCRIBE, NULL, 0);

	if (zmq_connect(ret, pubsub_addr+3) == -1) {
		g_warning("Failed to connect to PubSub on address %s: %s", pubsub_addr, zmq_strerror(zmq_errno()));

		ret = NULL;
		goto out;
	}

out:
	if (pubsub_addr) g_free(pubsub_addr);
	g_free(repreq_addr);
	return ret;
}

int main (int argc, char **argv)
{
	int ret = 0;

	GError* err = NULL;
	GOptionContext* ctx;

	void* zmq_ctx = NULL;
	struct op_services services;

	g_thread_init(NULL);

	ctx = g_option_context_new(" - A GStreamer backend daemon for Play");
	g_option_context_add_main_entries(ctx, entries, "");

	g_option_context_add_group(ctx, gst_init_get_option_group());

	if (!g_option_context_parse(ctx, &argc, &argv, &err)) {
		g_error("Option parsing failed: %s", err->message);
		ret = EXIT_FAILURE;
		goto out;
	}

	zmq_ctx = zmq_ctx_new();

	struct timer_closure closure = { NULL, NULL, NULL, FALSE, FALSE, };

	if (client_message) {
		char* address = zeromq_address_from_port("127.0.0.1", icecast_port);
		char* msg = util_send_reqrep_msg(zmq_ctx, client_message, address);

		if (msg) {
			g_print("%s\n", msg);
			g_free(msg);
		}

		ret = msg ? 0 : 1;
		g_free(address);
		goto out;
	}

	if (pubsub_listen) {
		if (!(closure.zmq_socket = create_pubsub_socket(zmq_ctx, icecast_port))) {
			goto out;
		}

		closure.pubsub_mode = TRUE;
	} else {
		if (!(services.pub_sub = pubsub_new(zmq_ctx, icecast_port))) {
			goto out;
		}

		for (struct parser_plugin_entry* pp_entry = parser_operations; pp_entry->friendly_name; pp_entry++) {
			pp_entry->context = &services;
		}
 
		struct parse_ctx* parser = parse_new();
		
		for (struct parser_plugin_entry* op = parser_operations; op->friendly_name; op++) {
			parse_register_plugin(parser, op);
		}

		closure.zmq_socket = create_server_socket(zmq_ctx, icecast_port);
		closure.parse_ctx = parser;
	}

	/* Server Mainloop */

	GMainLoop* main_loop = g_main_loop_new(NULL, FALSE);
	closure.main_loop = main_loop;

	g_timeout_add(250, handle_incoming_messages, &closure);

#ifdef G_OS_UNIX
	g_unix_signal_add(SIGINT, handle_sigint, &closure.should_quit);
	g_unix_signal_add(SIGTERM, handle_sigint, &closure.should_quit);
#endif

	g_warning("Starting Main Loop");
	g_main_loop_run(main_loop);
	g_warning("Bailing");

	if (!pubsub_listen) {
		parse_free(closure.parse_ctx);
		pubsub_free(services.pub_sub);
	}

out:
	if (closure.zmq_socket) util_close_socket(closure.zmq_socket);
	if (zmq_ctx) zmq_ctx_destroy(zmq_ctx);

	g_option_context_free(ctx);
	return ret;
}

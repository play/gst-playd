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
static char* client_message = NULL;
static int icecast_port = 8000;

static GOptionEntry entries[] = {
	 { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
	 { "send-message", 's', 0, G_OPTION_ARG_STRING, &client_message, "Send a message to a running gst_playd and exit", NULL },
	 { "port", 'p', 0, G_OPTION_ARG_INT, &icecast_port, "Set the port that Icecast will bind to", NULL },
	 { NULL },
};

struct timer_closure {
	void* zmq_socket;
	struct parse_ctx* parse_ctx;
	GMainLoop* main_loop;
	gboolean should_quit;
};

static struct parser_plugin_entry parser_operations[] = {
	{ "Ping", NULL, op_ping_new, op_ping_register, op_ping_free },
	{ NULL },
};

static char* zeromq_address_from_port(const char* address, int port)
{
	return g_strdup_printf("tcp://%s:%d", address, port + 10000);
}

static gboolean send_client_message(void* zmq_context, const char* message, const char* address)
{
	gboolean ret = TRUE;
	int linger = 15*1000;
	void* sock = zmq_socket(zmq_context, ZMQ_REQ);

	if (!sock) {
		g_warning("Failed to create socket: %s", zmq_strerror(zmq_errno()));
		return FALSE;
	}

	zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(int));

	g_print("Connecting to %s\n", address);
	if (zmq_connect(sock, address) == -1) {
		g_warning("Failed to connect: %s", zmq_strerror(zmq_errno()));
		ret = FALSE; goto out;
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

	g_print("Reply: %s\n", rep_text);
	g_free(rep_text);
	zmq_msg_close(&rep_msg);

out:
	util_close_socket(sock);
	return ret;
}

static void zmq_glib_free(void* to_free, void* hint)
{
	g_free(to_free);
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
	zmq_msg_init_data(&rep_msg, (void*)data, sizeof(char) * strlen(data), zmq_glib_free, NULL);
	zmq_msg_send(&rep_msg, zmq_sock, 0);
	zmq_msg_close(&rep_msg);

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

	while (handle_message(closure->zmq_socket, closure->parse_ctx) == 0) {
		g_debug("Processing new message");
	}

	return TRUE;
}

static gboolean handle_sigint(void* shouldquit)
{
	gboolean* should_quit = shouldquit;
	*should_quit = TRUE;

	return TRUE;
}

int main (int argc, char **argv)
{
	int ret = 0;

	GError* err = NULL;
	GOptionContext* ctx;

	void* zmq_ctx = NULL;
	void* sock = NULL;

	g_thread_init(NULL);

	ctx = g_option_context_new(" - A GStreamer backend daemon for Play");
	g_option_context_add_main_entries(ctx, entries, "");

	GOptionGroup* grp = gst_init_get_option_group();
	g_warning("grp = 0x%p", grp);
	g_option_context_add_group(ctx, grp);

	if (!g_option_context_parse(ctx, &argc, &argv, &err)) {
		g_error("Option parsing failed: %s", err->message);
		ret = EXIT_FAILURE;
		goto out;
	}

	zmq_ctx = zmq_ctx_new();
	char* address = zeromq_address_from_port("127.0.0.1", icecast_port);

	if (client_message) {
		ret = send_client_message(zmq_ctx, client_message, address) ? 0 : 1;
		goto out;
	}

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

	struct op_services* services = g_new0(struct op_services, 1);
	if (!(services->pub_sub = pubsub_new(zmq_ctx, icecast_port))) {
		g_free(services);
		goto out;
	}

	struct parse_ctx* parser = parse_new();
	
	parser_operations->context = op_services;
	for (struct parser_plugin_entry* op = parser_operations; op->friendly_name; op++) {
		parse_register_plugin(parser, op);
	}

	/* Server Mainloop */

	GMainLoop* main_loop = g_main_loop_new(NULL, FALSE);

	struct timer_closure closure = { sock, parser, main_loop, FALSE, };
	g_timeout_add(250, handle_incoming_messages, &closure);

#ifdef G_OS_UNIX
	g_unix_signal_add(SIGINT, handle_sigint, &closure.should_quit);
	g_unix_signal_add(SIGTERM, handle_sigint, &closure.should_quit);
#endif

	g_warning("Starting Main Loop");
	g_main_loop_run(main_loop);
	g_warning("Bailing");

	parse_free(parser);

	pubsub_free(services->pub_sub);

out:
	util_close_socket(sock);
	if (zmq_ctx) zmq_ctx_destroy(zmq_ctx);
	if (address) g_free(address);

	g_option_context_free(ctx);
	return ret;
}

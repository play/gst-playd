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
#include <string.h>
#include <sys/types.h>

#include <glib.h>
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
	void* zmq_socket;
	GMainLoop* main_loop;
	gboolean should_quit;
};

static char* zeromq_address_from_port(const char* address, int port)
{
    return g_strdup_printf("tcp://%s:%d", address, port + 10000);
}

static gboolean close_socket(void* sock)
{
	if (!sock) return TRUE;

	if (zmq_close(sock) == -1) {
		g_warning("Failed to close socket: %s", zmq_strerror(zmq_errno()));
		return FALSE;
	}

	return TRUE;
}

static gboolean send_client_message(void* zmq_context, const char* address, const char* message)
{
	gboolean ret = TRUE;
	int linger = 5*1000;

	void* sock = zmq_socket(zmq_context, ZMQ_REQ);

	if(!sock) {
	  g_warning("Failed to create socket: %s", zmq_strerror(zmq_errno()));
	  return FALSE;
	}

	zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(int));

	g_warning("Connecting to %s", address);
	if (zmq_connect(sock, address) == -1) {
		g_warning("Failed to connect: %s", zmq_strerror(zmq_errno()));
		ret = FALSE; goto out;
	}

	zmq_msg_t msg;
	size_t msg_size = sizeof(char) * strlen(message);
	zmq_msg_init_data(&msg, (void*)message, msg_size, NULL, NULL);
	zmq_msg_send(&msg, sock, ZMQ_DONTWAIT);
	zmq_msg_close(&msg);


	zmq_msg_t rep_msg;
	zmq_msg_init(&rep_msg);
	zmq_msg_recv(&rep_msg, sock, 0);
	g_warning("Reply: %s", (char*)zmq_msg_data(&rep_msg));
	zmq_msg_close(&rep_msg);
out:
	zmq_close(sock);
	return ret;
}

static gboolean handle_incoming_messages(gpointer user_data)
{
	gboolean ret = TRUE;

	struct timer_closure* closure = (struct timer_closure*) user_data;
	void* zmq_sock = closure->zmq_socket;
	zmq_msg_t msg;

	if (closure->should_quit) {
		g_main_loop_quit(closure->main_loop);
		return FALSE;
	}

	zmq_msg_init(&msg);
	//g_warning("Tick");
	if (zmq_msg_recv(&msg, zmq_sock, ZMQ_DONTWAIT) == -1) {
		switch (zmq_errno()) {
		case EAGAIN:
		case EINTR:
		case EFSM:
			ret = TRUE;
			goto out;
		default:
			g_warning("Failed to recieve message: %s", zmq_strerror(zmq_errno()));
			g_main_loop_quit(closure->main_loop);
			ret = FALSE;
			goto out;
		}
	}

	g_warning("Message recieved: %s", (char*)zmq_msg_data(&msg));

	zmq_msg_t rep_msg;
	const char* data = "Yo.";
	size_t msg_size = sizeof(char) * strlen(data);
	zmq_msg_init_data(&rep_msg, (void*)data, msg_size, NULL, NULL);
	zmq_msg_send(&rep_msg, zmq_sock, ZMQ_DONTWAIT);
	zmq_msg_close(&rep_msg);

out:
zmq_msg_close(&msg);
	return ret;
}

static void handle_sigint(void* shouldquit)
{
	gboolean* should_quit = shouldquit;
	*should_quit = TRUE;
}

int main (int argc, char **argv)
{
	int ret = 0;

	GError* err = NULL;
	GOptionContext* ctx;

	void* zmq_ctx = NULL;
	void* sock = NULL;

	char* address = zeromq_address_from_port("*", icecast_port);

	ctx = g_option_context_new(" - A GStreamer backend daemon for Play");
	g_option_context_add_main_entries(ctx, entries, "");

	if (!g_option_context_parse(ctx, &argc, &argv, &err)) {
		g_error("Option parsing failed: %s", err->message);
		ret = EXIT_FAILURE;
		goto out;
	}

	zmq_ctx = zmq_ctx_new();

	sock = zmq_socket(zmq_ctx, ZMQ_REP);

	if(!sock) {
	  g_warning("Failing to create socket %s", zmq_strerror(zmq_errno()));
	}

        int linger = 5*1000;
	zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(int));

	if (client_message) {
	    char* client_address = zeromq_address_from_port("localhost", icecast_port);
	        ret = send_client_message(zmq_ctx, client_address, client_message) ? 0 : 1;
		free((void*)client_address);
		goto out;
	}


	if (zmq_bind(sock, address) == -1) {
	  g_warning("Failed to start server on address %s: %s", address, zmq_strerror(zmq_errno()));
	  goto out;
	}

	/* Server Mainloop */

	GMainLoop* main_loop = g_main_loop_new(NULL, FALSE);

	struct timer_closure closure = { sock, main_loop, FALSE, };
	g_timeout_add(250, handle_incoming_messages, &closure);

	g_unix_signal_add(SIGINT, handle_sigint, &closure.should_quit);
	g_unix_signal_add(SIGTERM, handle_sigint, &closure.should_quit);

	g_warning("Starting Main Loop");
	g_main_loop_run(main_loop);
	g_warning("Bailing");

out:
	close_socket(sock);
	if (zmq_ctx) zmq_ctx_destroy(zmq_ctx);
	if (address) g_free((void*)address);

	g_option_context_free(ctx);
	return ret;
}

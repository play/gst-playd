/*
   pubsub.c - API for sending messages over the ZeroMQ PUB socket

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
#include <sys/uio.h>
#include <zmq.h>

#include "pubsub.h"
#include "utility.h"

struct pubsub_ctx {
	void* sock;
};

static char* pubsub_address_from_port(const char* address, int port)
{
	return g_strdup_printf("tcp://%s:%d", address, port + 10001);
}

struct pubsub_ctx* pubsub_new(void* zmq_context, int icecast_port)
{
	struct pubsub_ctx* ret = g_new0(struct pubsub_ctx, 1);
	int linger = 15*1000;

	ret->sock = zmq_socket(zmq_context, ZMQ_PUB);
	zmq_setsockopt(ret->sock, ZMQ_LINGER, &linger, sizeof(int));

	char* addr = pubsub_address_from_port("127.0.0.1", icecast_port);
	if (zmq_bind(ret->sock, addr) == -1) {
		g_warning("Failed to start server on address %s: %s", addr, zmq_strerror(zmq_errno()));

		pubsub_free(ret);
		ret = NULL;
		goto out;
	}

out:
	g_free(addr);
	return ret;
}

void pubsub_free(struct pubsub_ctx* ctx)
{
	util_close_socket(ctx->sock);
	g_free(ctx);
}

gboolean pubsub_send_message(struct pubsub_ctx* ctx, const char* message)
{
	zmq_msg_t msg;
	zmq_msg_init_data(&msg, (void*) message, sizeof(char) * strlen(message), NULL, NULL);
	zmq_msg_send(&msg, ctx->sock, 0);
	zmq_msg_close(&msg);

	return TRUE;
}

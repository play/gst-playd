/*
   pubsub.h - API for sending messages over the ZeroMQ PUB socket

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

#ifndef _PUBSUB_H
#define _PUBSUB_H

struct pubsub_ctx;

struct pubsub_ctx* pubsub_new(void* zmq_context, int icecast_port);
void pubsub_free(struct pubsub_ctx* ctx);
gboolean pubsub_send_message(struct pubsub_ctx* ctx, const char* message);

#endif

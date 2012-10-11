/*
   operations.c - Message handlers

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
#include "utility.h"
#include "gst-util.h"
#include "op_services.h"

struct message_dispatch_entry ping_messages[] = {
	{ "PING", op_ping_parse },
	{ NULL },
};

struct message_dispatch_entry control_messages[] = {
	{ "PUBSUB", op_pubsub_parse },
	{ NULL },
};

struct message_dispatch_entry playback_messages[] = {
	{ "TAGS", op_tags_parse },
	{ NULL },
};


/*
 * Ping
 */

void* op_ping_new(void* services)
{
	return services;
}

gboolean op_ping_register(void* ctx, struct message_dispatch_entry** entries)
{
	*entries = ping_messages;
	return TRUE;
}

void op_ping_free(void* dontcare)
{
}

char* op_ping_parse(const char* param, void* ctx)
{
	struct op_services* services = (struct op_services*)ctx;

	if (!param) param = "(none)";
	char* ret = g_strdup_printf("OK Message was %s", param);

	pubsub_send_message(services->pub_sub, ret);
	return ret;
}


/*
 * Control Messages
 */

void* op_control_new(void* op_services)
{
	return op_services;
}

gboolean op_control_register(void* ctx, struct message_dispatch_entry** entries)
{
	*entries = control_messages;
	return TRUE;
}

void op_control_free(void* dontcare)
{
}

char* op_pubsub_parse(const char* param, void* ctx)
{
	struct op_services* services = (struct op_services*)ctx;
	return g_strdup_printf("OK %s", pubsub_get_address(services->pub_sub));
}


struct playback_ctx {
	struct op_services* services;
};

/*
 * Playback Messages
 */

void* op_playback_new(void* op_services)
{
	return op_services;
}

gboolean op_playback_register(void* ctx, struct message_dispatch_entry** entries)
{
	*entries = playback_messages;
	return TRUE;
}

void op_playback_free(void* dontcare)
{
}

static void on_new_pad_tags(GstElement* dec, GstPad* pad, GstElement* fakesink) 
{
	  GstPad *sinkpad;

	  sinkpad = gst_element_get_static_pad(fakesink, "sink"); 

	  if (!gst_pad_is_linked (sinkpad)) {
		  if (gst_pad_link(pad, sinkpad) != GST_PAD_LINK_OK)  {
			  g_error("Failed to link pads!");
		  }
	  }
	    
	  gst_object_unref (sinkpad);
}

char* op_tags_parse(const char* param, void* ctx)
{
	GstElement* pipe;
	GstElement* dec;
	GstElement* sink;

	GstMessage* msg;
	char* ret;

	pipe = gst_pipeline_new("pipeline");
	dec = gst_element_factory_make("uridecodebin", NULL); 

	g_object_set(dec, "uri", param, NULL);

	gst_bin_add (GST_BIN (pipe), dec);
	sink = gst_element_factory_make("fakesink", NULL); gst_bin_add (GST_BIN (pipe), sink);
	g_signal_connect(dec, "pad-added", G_CALLBACK (on_new_pad_tags), sink);

	gst_element_set_state(pipe, GST_STATE_PAUSED);

	GHashTable* tag_table = NULL;

	while (TRUE) {
		GstTagList *tags = NULL;

		msg = gst_bus_timed_pop_filtered(GST_ELEMENT_BUS (pipe), GST_CLOCK_TIME_NONE,
			GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_TAG | GST_MESSAGE_ERROR);

		/* error or async_done */ 
		if (GST_MESSAGE_TYPE (msg) != GST_MESSAGE_TAG) {
			break;
		}

		gst_message_parse_tag(msg, &tags);
		tag_table = gsu_tags_to_hash_table(tags);

		gst_tag_list_free(tags);
		gst_message_unref(msg);
	}

	char* table_data = util_hash_table_as_string(tag_table);
	ret = g_strdup_printf("OK\n%s", table_data);
	g_free(table_data);
	
	g_hash_table_destroy(tag_table);
	return ret;
}

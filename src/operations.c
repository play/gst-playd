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
	{ "QUIT", op_quit_parse },
	{ NULL },
};

struct message_dispatch_entry playback_messages[] = {
	{ "TAGS", op_tags_parse },
	{ "PLAY", op_play_parse },
	{ "STOP", op_stop_parse },
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

char* op_quit_parse(const char* param, void* ctx)
{
	struct op_services* services = (struct op_services*)ctx;
	*services->should_quit = TRUE;

	return strdup("OK");
}


/*
 * Playback Messages
 */

struct source_item {
	char* uri;
	GstElement* element;
};

struct playback_ctx {
	struct op_services* services;
	GstElement* pipeline;

	GstElement* mux;
	GstElement* audio_sink;

	GSList* sources;
};

static void on_new_pad_link(GstElement* src, GstPad* pad, GstElement* mux) 
{
	GstPad* mux_pad = gst_element_get_request_pad(mux, "src%d");
	if (!mux_pad) {
		g_error("Couldn't get request pad from mux!");
		return;
	}

	if (!gst_pad_link(pad, mux_pad)) {
		g_error("Couldn't link source to mux!");
		return;
	}
}

static struct source_item* source_new_and_link(const char* uri, GstElement* mux)
{
	struct source_item* ret = g_new0(struct source_item, 1);

	ret->uri = strdup(uri);
	ret->element = gst_element_factory_make("uridecodebin", NULL);

	g_object_set(ret->element, "uri", uri, NULL);
	g_signal_connect(ret, "pad-added", G_CALLBACK(on_new_pad_link), mux);

	return ret;
}

static void source_pad_remove_foreach(gpointer item, gpointer user_data)
{
	GstPad* source_pad = GST_PAD_CAST(item);
	GstElement* mux = GST_ELEMENT_CAST(user_data);
	GstPad* mux_pad = gst_pad_get_peer(source_pad);

	gst_pad_unlink(source_pad, mux_pad);
	gst_element_release_request_pad(mux, mux_pad);
	g_object_unref(GST_OBJECT(mux_pad));
}

static void source_free_and_unlink(struct source_item* item, GstElement* mux)
{
	GstIterator* pad_iterator;

	pad_iterator = gst_element_iterate_sink_pads(item->element);
	gst_iterator_foreach(pad_iterator, source_pad_remove_foreach, mux);

	g_free(item->uri);
	g_free(item);
}

void* op_playback_new(void* op_services)
{
	GError* error = NULL;
	struct playback_ctx* ret = g_new0(struct playback_ctx, 1);

	ret->services = op_services;
	if (!(ret->audio_sink = gst_parse_launch("audioconvert ! osxaudiosink", &error))) {
		g_error("Couldn't create audio sink: %s", error->message);
		return NULL;
	}

	if (!(ret->mux = gst_element_factory_make("adder", NULL))) {
		g_error("Couldn't create mixer");
		return NULL;
	}

	ret->pipeline = gst_pipeline_new("pipeline");

	gst_bin_add_many(GST_BIN_CAST(ret->pipeline), ret->mux, ret->audio_sink, NULL);
	return ret;
}

gboolean op_playback_register(void* ctx, struct message_dispatch_entry** entries)
{
	if (!ctx) {
		return FALSE;
	}

	*entries = playback_messages;
	return TRUE;
}

void op_playback_free(void* ctx)
{
	struct playback_ctx* context = (struct playback_ctx*)ctx;
	GSList* iter = context->sources;

	while (iter) {
		source_free_and_unlink((struct source_item*)iter->data, context->mux);
		iter = g_slist_next(iter);
	}

	g_object_unref(GST_OBJECT(context->pipeline));
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
	char* ret = NULL;

	pipe = gst_pipeline_new("pipeline");
	dec = gst_element_factory_make("uridecodebin", NULL); 

	g_object_set(dec, "uri", param, NULL);

	gst_bin_add (GST_BIN (pipe), dec);
	sink = gst_element_factory_make("fakesink", NULL); gst_bin_add (GST_BIN (pipe), sink);
	g_signal_connect(dec, "pad-added", G_CALLBACK (on_new_pad_tags), sink);

	gst_element_set_state(pipe, GST_STATE_PAUSED);

	GHashTable* tag_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	while (TRUE) {
		GstTagList *tags = NULL;

		msg = gst_bus_timed_pop_filtered(GST_ELEMENT_BUS (pipe), GST_CLOCK_TIME_NONE,
			GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_TAG | GST_MESSAGE_ERROR);

		if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
			GError* error = NULL;
			gst_message_parse_error(msg, &error, NULL);
			ret = g_strdup_printf("FAIL %s", error->message);
			break;
		}

		/* error or async_done */ 
		if (GST_MESSAGE_TYPE (msg) != GST_MESSAGE_TAG) {
			break;
		}

		gst_message_parse_tag(msg, &tags);
		gsu_tags_to_hash_table(tags, tag_table);

		gst_tag_list_free(tags);
		gst_message_unref(msg);
	}

	if (!ret || g_hash_table_size(tag_table) > 0) {
		if (ret) g_free(ret);

		char* table_data = util_hash_table_as_string(tag_table);
		ret = g_strdup_printf("OK\n%s", table_data);
		g_free(table_data);
	}

	g_hash_table_destroy(tag_table);
	return ret;
}

char* op_play_parse(const char* param, void* ctx)
{
	return NULL;
}

char* op_stop_parse(const char* param, void* ctx)
{
	return NULL;
}

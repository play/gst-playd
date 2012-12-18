/*
   play.c - Play pipeline message handlers

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
#include "utility.h"
#include "gst-util.h"
#include "op_services.h"

#include "operations/play.h"

static struct message_dispatch_entry playback_messages[] = {
	{ "TAGS", op_tags_parse },
	{ "PLAY", op_play_parse },
	{ "STOP", op_stop_parse },
	{ "DUMPGRAPH", op_dumpgraph_parse },
	{ NULL },
};

struct source_item {
	char* uri;
	GDateTime* created_at;

	GstElement* element;
	GstElement* ac;
};

struct playback_ctx {
	struct op_services* services;
	GstElement* pipeline;

	GstElement* mux;
	GstElement* audio_sink;

	GSList* sources;
};


static void on_new_source_pad_link(GstElement* src, GstPad* pad, GstElement* mux)
{
	if (!gst_element_link(src, mux)) {
		g_error("Couldn't link source to mux");
	}
}

static struct source_item* source_new_and_link(const char* uri, GstElement* pipeline, GstElement* mux)
{
	struct source_item* ret = g_new0(struct source_item, 1);

	ret->uri = strdup(uri);
	ret->created_at = g_date_time_new_now_utc();
	ret->element = gst_element_factory_make("uridecodebin", NULL);

	ret->ac = gst_element_factory_make("audioconvert", NULL);
	gst_bin_add(GST_BIN(pipeline), ret->ac);
	gst_element_link(ret->ac, mux);

	gst_bin_add(GST_BIN(pipeline), ret->element);
	g_object_set(ret->element, "uri", uri, NULL);
	g_signal_connect(ret->element, "pad-added", G_CALLBACK(on_new_source_pad_link), ret->ac);

	GstState current, pending;
	gst_element_get_state(pipeline, &current, &pending, 0);

	GstElement* target = ret->element;
	if (pending != GST_STATE_PLAYING && current != GST_STATE_PLAYING) {
		target = pipeline;
	} 

	if (gst_element_set_state(target, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		g_error("Couldn't move element state to PLAYING");
	}

	return ret;
}

static void source_free_and_unlink(struct source_item* item, GstElement* pipeline, GstElement* mux)
{
	if (gst_element_set_state(item->element, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		g_error("Couldn't move element state to READY");
	}

	/* NB: If we simply unlink element, ac, and mux, we'll leave behind a
	 * request sink on the mux where the source used to be. Since we're 
	 * playing, this will cause us to fault out and playback to stop.
	 *
	 * So here, we grab the mux's request pad via the audioconvert source, 
	 * then remove it after we do the unlink. */
	GstPad* ac_src = gst_element_get_static_pad(item->ac, "src");
	GstPad* mux_sink = gst_pad_get_peer(ac_src);

	gst_element_unlink_many(item->element, item->ac, mux, NULL);

	gst_element_release_request_pad(mux, mux_sink);

	gst_bin_remove(GST_BIN(pipeline), item->element);
	gst_bin_remove(GST_BIN(pipeline), item->ac);
		
	g_date_time_unref(item->created_at);
	g_free(item->uri);
	g_free(item);
}

static guint source_item_to_id(struct source_item* item)
{
	char to_hash[2048];
	sprintf(to_hash, "%s 0x%p %lu", item->uri, item, g_date_time_to_unix(item->created_at));

	return g_str_hash(to_hash);
}

static struct source_item* source_item_from_id(GSList* item_list, guint id)
{
	GSList* haystack = item_list;

	while (haystack) {
		struct source_item* needle = haystack->data;
		if (source_item_to_id(needle) == id) {
			return needle;
		}

		haystack = g_slist_next(haystack);
	}

	return NULL;
}

static gboolean playback_bus_callback(GstBus* bus, GstMessage* message, gpointer userdata)
{
	struct playback_ctx* ctx = userdata;

	GError* err = NULL;
	char* prefix = NULL;

	g_warning ("Got pipeline bus message of type %s", GST_MESSAGE_TYPE_NAME(message));
	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_ERROR:
		prefix = "ERROR";
		gst_message_parse_error(message, &err, NULL);
		break;
	case GST_MESSAGE_WARNING:
		prefix = "WARNING";
		gst_message_parse_warning(message, &err, NULL);
		break;
	case GST_MESSAGE_INFO:
		prefix = "INFO";
		gst_message_parse_info(message, &err, NULL);
		break;
	default:
		return TRUE;
	}

	char* msg = g_strdup_printf("%s: %s", prefix, err->message);
	g_warning("Writing message to bus: %s", msg);
	pubsub_send_message(ctx->services->pub_sub, msg);

	return TRUE;
}

void* op_playback_new(void* op_services)
{
	GError* error = NULL;
	struct playback_ctx* ret = g_new0(struct playback_ctx, 1);

	ret->services = op_services;
	if (!(ret->audio_sink = gst_element_factory_make("osxaudiosink", NULL))) {
		g_error("Couldn't create audio sink: %s", error->message);
		return NULL;
	}

	if (!(ret->mux = gst_element_factory_make("adder", NULL))) {
		g_error("Couldn't create mixer");
		return NULL;
	}

	ret->pipeline = gst_pipeline_new("pipeline");

	GstElement* ac = gst_element_factory_make("audioconvert", NULL);
	gst_bin_add_many(GST_BIN_CAST(ret->pipeline), ret->mux, ac, ret->audio_sink, NULL);

	if (!(gst_element_link_many(ret->mux, ac, ret->audio_sink, NULL))) {
		g_error("Couldn't link mux");
		return NULL;
	}

	GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(ret->pipeline));
	gst_bus_add_watch(bus, playback_bus_callback, ret);
	gst_object_unref(bus);

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
		source_free_and_unlink((struct source_item*)iter->data, context->pipeline, context->mux);
		iter = g_slist_next(iter);
	}

	gst_element_set_state(context->pipeline, GST_STATE_READY);
	g_object_unref(GST_OBJECT(context->pipeline));

	g_slist_free(context->sources);
	g_free(context);
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
	struct playback_ctx* context = (struct playback_ctx*)ctx;

	struct source_item* to_add;
	if (!(to_add = source_new_and_link(param, context->pipeline, context->mux))) {
		return g_strdup_printf("FAIL Can't load source: %s", param);
	}

	context->sources = g_slist_prepend(context->sources, to_add);
	return g_strdup_printf("OK player id: %u", source_item_to_id(to_add));
}

char* op_stop_parse(const char* param, void* ctx)
{
	struct playback_ctx* context = (struct playback_ctx*)ctx;

	guint id = (guint) atoll(param);
	struct source_item* to_remove = source_item_from_id(context->sources, id);

	if (!to_remove) {
		return strdup("FAIL id is invalid");
	}

	context->sources = g_slist_remove(context->sources, to_remove);

	source_free_and_unlink(to_remove, context->pipeline, context->mux);
	return g_strdup_printf("OK player id: %u", id);
}

char* op_dumpgraph_parse(const char* param, void* ctx)
{
	struct playback_ctx* context = (struct playback_ctx*)ctx;

	GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(context->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, param);
	return strdup("OK");
}

/*
   gst-util.c - GStreamer helper functions

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
#include <gst/gst.h>
#include <string.h>

#include "gst-util.h"

static void tag_to_hash_table(const GstTagList * list, const gchar * tag, gpointer user_data) 
{
	char* value;
	int num = gst_tag_list_get_tag_size (list, tag); 
	GHashTable* ret = (GHashTable*)user_data;

	for (int i = 0; i < num; ++i) {
		const GValue *val = gst_tag_list_get_value_index (list, tag, i); 

		if (G_VALUE_HOLDS_STRING (val)) {
			value = strdup(g_value_get_string(val));
		} else if (G_VALUE_HOLDS_UINT (val)) {
			value = g_strdup_printf("%u", g_value_get_uint(val));
		} else if (G_VALUE_HOLDS_UINT64(val)) {
			value = g_strdup_printf("%lu64", g_value_get_uint64(val));
		} else if (G_VALUE_HOLDS_DOUBLE (val)) {
			value = g_strdup_printf("%f", g_value_get_double(val));
		} else if (G_VALUE_HOLDS_BOOLEAN (val)) { 
			value = strdup(g_value_get_boolean (val) ? "true" : "false");
		} else if (GST_VALUE_HOLDS_BUFFER (val)) {
			GstBuffer* buf = gst_value_get_buffer(val);
			value = g_new0 (char, GST_BUFFER_SIZE(buf) + 1);
			memcpy(value, GST_BUFFER_DATA(buf), sizeof(char) * GST_BUFFER_SIZE(buf));
		} else if (GST_VALUE_HOLDS_DATE (val)) { 
			value = g_new0(char, sizeof(char) * 128);
			g_date_strftime(value, 50, "%F", gst_value_get_date (val));
		/*} else if (GST_VALUE_HOLDS_DATE_TIME (val)) { 
			value = gst_date_time_to_iso8601_string((GstDateTime*)val); */
		} else {
			value = g_strdup_printf ("tag of type ’%s’", G_VALUE_TYPE_NAME (val)); 
		}

		g_warning("Found tag: %s => %s", tag, value);
		g_hash_table_insert(ret, g_strdup_printf("%s_%d", tag, i), value);
	}
}

GHashTable* gsu_tags_to_hash_table(const GstTagList* tags)
{
	GHashTable* ret = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	gst_tag_list_foreach(tags, tag_to_hash_table, ret);

	return ret;
}

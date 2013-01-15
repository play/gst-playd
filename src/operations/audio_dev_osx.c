/*
   audio_dev_osx.c - Audio device operations for OS X

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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/AudioHardware.h>

#include "parser.h"
#include "ping.h"
#include "utility.h"
#include "op_services.h"

#include "operations/audio_dev_osx.h"

static struct message_dispatch_entry audio_dev_messages[] = { 
	{ "LISTDEVICE", op_listdevice_parse },
	{ NULL },
};

GHashTable* enumerate_audio_devices(gboolean only_input_devices, GError** error);

void* op_audiodev_new(void* op_services)
{
	return op_services;
}

char* op_listdevice_parse(const char* param, void* ctx)
{
	gboolean only_input = (strcmp(param, "INPUT") == 0);
	GError* err = NULL;
	char* ret;

	GHashTable* device_list = enumerate_audio_devices(only_input, &err);

	if (err) {
		ret = strdup(err->message);
		g_error_free(err);
		goto out;
	}

	char* table_data = util_int_hash_table_as_string(device_list);
	ret = g_strdup_printf("OK\n%s", table_data);
	g_free(table_data);
	g_hash_table_destroy(device_list);

out:
	return ret;
}

gboolean op_audiodev_register(void* ctx, struct message_dispatch_entry** entries)
{
	if (!ctx) {
		return FALSE;
	}

	*entries = audio_dev_messages;
	return TRUE;
}

void op_audiodev_free(void* ctx)
{
}

/* This code is based on
 * http://stackoverflow.com/questions/4575408/audioobjectgetpropertydata-to-get-a-list-of-input-devices */

GHashTable* enumerate_audio_devices(gboolean only_input_devices, GError** error)
{
	gchar* error_message = NULL;
	AudioDeviceID* audio_devices = NULL;
	OSStatus status;
	GHashTable* ret = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, g_free);

	AudioObjectPropertyAddress property_address = { 
		kAudioHardwarePropertyDevices, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster,
	};

	property_address.mScope = only_input_devices ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;

	UInt32 data_size = 0;
	status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &property_address, 0, NULL, &data_size);

	UInt32 device_count = (UInt32)(data_size / sizeof(AudioDeviceID));
	audio_devices = (AudioDeviceID*) g_new(AudioDeviceID, device_count);

	status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_address, 0, NULL, &data_size, audio_devices);
	if(status != kAudioHardwareNoError) {
		error_message = g_strdup_printf("AudioObjectGetPropertyData (kAudioHardwarePropertyDevices) failed: %i\n", status);
		goto out;
	}

	for(int i = 0; i < device_count; ++i) {
		/* Determine if the device is a matching device (it is an 
		 * input device if it has input channels) */
		data_size = 0;
		property_address.mSelector = kAudioDevicePropertyStreamConfiguration;
		status = AudioObjectGetPropertyDataSize(audio_devices[i], &property_address, 0, NULL, &data_size);
		if(kAudioHardwareNoError != status) {
			g_warning("AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration) failed: %i\n", status);
			continue;
		}

		AudioBufferList *bufferList = (AudioBufferList*) g_new0(char, data_size);
		if(NULL == bufferList) {
			g_warning("Unable to allocate memory");
			break;
		}

		status = AudioObjectGetPropertyData(audio_devices[i], &property_address, 0, NULL, &data_size, bufferList);
		if(kAudioHardwareNoError != status || 0 == bufferList->mNumberBuffers) {
			if(kAudioHardwareNoError != status)
				g_warning("AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration) failed: %i\n", status);

			free(bufferList), bufferList = NULL;
			continue;           
		}

		/* Query device name */
		CFStringRef deviceName = NULL;
		data_size = sizeof(deviceName);
		property_address.mSelector = kAudioDevicePropertyDeviceNameCFString;

		status = AudioObjectGetPropertyData(audio_devices[i], &property_address, 0, NULL, &data_size, &deviceName);
		if(kAudioHardwareNoError != status) {
			g_warning("AudioObjectGetPropertyData (kAudioDevicePropertyDeviceNameCFString) failed: %i\n", status);
			continue;
		}

		char* val = g_new0(char, 512);
		CFStringGetCString(deviceName, val, 512, kCFStringEncodingUTF8);
		g_hash_table_insert(ret, &audio_devices[i], val);
	}

	g_free(audio_devices);

out:
	if (error_message) {
		g_set_error(error, AUDIODEV_DOMAIN, 1, "%s", error_message);
	}

	return ret;
}

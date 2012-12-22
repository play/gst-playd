#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/AudioHardware.h>

CFArrayRef CreateInputDeviceArray()
{
	AudioObjectPropertyAddress propertyAddress = { 
		kAudioHardwarePropertyDevices, 
		kAudioObjectPropertyScopeGlobal, 
		kAudioObjectPropertyElementMaster 
	};

	UInt32 dataSize = 0;
	OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize);
	if(kAudioHardwareNoError != status) {
		fprintf(stderr, "AudioObjectGetPropertyDataSize (kAudioHardwarePropertyDevices) failed: %i\n", status);
		return NULL;
	}

	UInt32 deviceCount = (UInt32)(dataSize / sizeof(AudioDeviceID));

	AudioDeviceID *audioDevices = (AudioDeviceID *)(malloc(dataSize));
	if(NULL == audioDevices) {
		fputs("Unable to allocate memory", stderr);
		return NULL;
	}

	status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize, audioDevices);
	if(kAudioHardwareNoError != status) {
		fprintf(stderr, "AudioObjectGetPropertyData (kAudioHardwarePropertyDevices) failed: %i\n", status);
		free(audioDevices), audioDevices = NULL;
		return NULL;
	}

	CFMutableArrayRef inputDeviceArray = CFArrayCreateMutable(kCFAllocatorDefault, deviceCount, &kCFTypeArrayCallBacks);
	if(NULL == inputDeviceArray) {
		fputs("CFArrayCreateMutable failed", stderr);
		free(audioDevices), audioDevices = NULL;
		return NULL;
	}

	// Iterate through all the devices and determine which are input-capable
	propertyAddress.mScope = kAudioDevicePropertyScopeInput;
	for(UInt32 i = 0; i < deviceCount; ++i) {
		// Query device UID
		CFStringRef deviceUID = NULL;
		dataSize = sizeof(deviceUID);
		propertyAddress.mSelector = kAudioDevicePropertyDeviceUID;
		status = AudioObjectGetPropertyData(audioDevices[i], &propertyAddress, 0, NULL, &dataSize, &deviceUID);
		if(kAudioHardwareNoError != status) {
			fprintf(stderr, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceUID) failed: %i\n", status);
			continue;
		}

		// Query device name
		CFStringRef deviceName = NULL;
		dataSize = sizeof(deviceName);
		propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
		status = AudioObjectGetPropertyData(audioDevices[i], &propertyAddress, 0, NULL, &dataSize, &deviceName);
		if(kAudioHardwareNoError != status) {
			fprintf(stderr, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceNameCFString) failed: %i\n", status);
			continue;
		}

		// Query device manufacturer
		CFStringRef deviceManufacturer = NULL;
		dataSize = sizeof(deviceManufacturer);
		propertyAddress.mSelector = kAudioDevicePropertyDeviceManufacturerCFString;
		status = AudioObjectGetPropertyData(audioDevices[i], &propertyAddress, 0, NULL, &dataSize, &deviceManufacturer);
		if(kAudioHardwareNoError != status) {
			fprintf(stderr, "AudioObjectGetPropertyData (kAudioDevicePropertyDeviceManufacturerCFString) failed: %i\n", status);
			continue;
		}

		// Determine if the device is an input device (it is an input device if it has input channels)
		dataSize = 0;
		propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
		status = AudioObjectGetPropertyDataSize(audioDevices[i], &propertyAddress, 0, NULL, &dataSize);
		if(kAudioHardwareNoError != status) {
			fprintf(stderr, "AudioObjectGetPropertyDataSize (kAudioDevicePropertyStreamConfiguration) failed: %i\n", status);
			continue;
		}

		AudioBufferList *bufferList = (AudioBufferList *)(malloc(dataSize));
		if(NULL == bufferList) {
			fputs("Unable to allocate memory", stderr);
			break;
		}

		status = AudioObjectGetPropertyData(audioDevices[i], &propertyAddress, 0, NULL, &dataSize, bufferList);
		if(kAudioHardwareNoError != status || 0 == bufferList->mNumberBuffers) {
			if(kAudioHardwareNoError != status)
				fprintf(stderr, "AudioObjectGetPropertyData (kAudioDevicePropertyStreamConfiguration) failed: %i\n", status);
			free(bufferList), bufferList = NULL;
			continue;           
		}

		free(bufferList), bufferList = NULL;

		// Add a dictionary for this device to the array of input devices
		CFStringRef keys    []  = { CFSTR("deviceUID"),     CFSTR("deviceName"),    CFSTR("deviceManufacturer") };
		CFStringRef values  []  = { deviceUID,              deviceName,             deviceManufacturer };

		CFDictionaryRef deviceDictionary = CFDictionaryCreate(kCFAllocatorDefault, 
				(const void **)(keys), 
				(const void **)(values), 
				3,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks);


		CFArrayAppendValue(inputDeviceArray, deviceDictionary);

		CFRelease(deviceDictionary), deviceDictionary = NULL;
	}

	free(audioDevices), audioDevices = NULL;

	// Return a non-mutable copy of the array
	CFArrayRef copy = CFArrayCreateCopy(kCFAllocatorDefault, inputDeviceArray);
	CFRelease(inputDeviceArray), inputDeviceArray = NULL;

	return copy;
}

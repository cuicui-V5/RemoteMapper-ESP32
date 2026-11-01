#pragma once

#define FIRMWARE_NAME       "RemoteMapper-ESP32"
#ifdef REMOTEMAPPER_MACOS
#define FIRMWARE_VERSION    "1.2.1-macos.13"
#define HARDWARE_TARGET     "ESP32-S3-Zero-N8R8"
#else
#define FIRMWARE_VERSION    "1.2.1"
#define HARDWARE_TARGET     "ESP32-S3-N16R8"
#endif
#define AUTHOR              "RemoteMapper Project"

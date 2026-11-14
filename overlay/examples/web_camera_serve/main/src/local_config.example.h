#pragma once

/* The Seeed Wio-WM6108 guide specifies the US regulatory profile. */
#define COUNTRY_CODE "US"

/* Leave probe-only enabled to identify the sensor without joining a network. */
#define CAMERA_PROBE_ONLY 1

/* Choose FRAMESIZE_QVGA (320x240) or FRAMESIZE_VGA (640x480). */
#define CAMERA_FRAME_SIZE FRAMESIZE_QVGA

/* For streaming, set these locally and change CAMERA_PROBE_ONLY to 0.
 * Credentials are compiled into the local firmware image, not stored in Git.
 */
#define HALOW_SSID "YOUR_HA_LOW_SSID"
#define HALOW_PASSPHRASE "YOUR_SAE_PASSPHRASE"

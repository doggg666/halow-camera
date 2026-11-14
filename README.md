# HaLow Camera

Seeed XIAO ESP32-S3 Sense camera node with a Wio-WM6108 Wi-Fi HaLow module. Based on Seeed's [mm-iot-esp32](https://github.com/Seeed-Studio/mm-iot-esp32) `web_camera_serve` example, pinned to commit `b8b7452ddbe2cc9b7a4d94e22ae6f4653c596821`.

The OV3660 camera was confirmed on the test board by sensor PID `0x3660`. The page at `http://CAMERA-IP/` shows the live feed, received bitrate, peak bitrate, frame rate, and a selector for 320 × 240 or 640 × 480. The direct MJPEG endpoint is `http://CAMERA-IP/stream`; it also accepts `?size=qvga` or `?size=vga`.

## Get the source

Run `sh setup.sh` from this repository. It clones Seeed's SDK at the pinned commit into `mm-iot-esp32/` and copies the files in `overlay/` over the matching upstream paths. The vendor SDK stays in its own Git checkout so its origin and local changes are visible.

## Configure and build

The Seeed SDK pins ESP-IDF 5.1.1. Use a complete ESP-IDF Git checkout with submodules and the matching ESP32-S3 compiler. Then:

```sh
cp mm-iot-esp32/examples/web_camera_serve/main/src/local_config.example.h \
   mm-iot-esp32/examples/web_camera_serve/main/src/local_config.h
# Edit local_config.h: set HALOW_SSID and HALOW_PASSPHRASE.
# Set CAMERA_PROBE_ONLY to 0 for HaLow streaming, or leave it at 1 to identify the sensor.
export MMIOT_ROOT="$PWD/mm-iot-esp32"
export COUNTRY_CODE=US
cd mm-iot-esp32/examples/web_camera_serve
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

`local_config.h` is ignored by Git. It contains private network credentials. Streaming firmware binaries also contain those credentials, so keep them private. Use the regulatory country code appropriate to the module and deployment location; the tested unit used `US`.

The project does not yet implement automatic discovery or playback on the CrowPanel. Its current camera stream can be opened from a browser on a network that can reach the HaLow station.

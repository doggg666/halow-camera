/*
 * Copyright 2022-2023 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

/**
 * @file
 * @brief Throughput measurement using iperf.
 *
 * The Iperf parameters are specified using the defines in the file. Additional defines in
 * @c loadconfig.c and @c mm_app_common.c are used to configure the network stack and WLAN
 * interface.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This file demonstrates how to run iperf using the Morse Micro WLAN API.
 */


#include <endian.h>
#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mmipal.h"

#include "mm_app_common.h"

#include <esp_system.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "esp_http_server.h"

#if __has_include("local_config.h")
#include "local_config.h"
#endif

#ifndef CAMERA_FRAME_SIZE
#define CAMERA_FRAME_SIZE FRAMESIZE_QVGA
#endif

#ifndef CAMERA_PROBE_ONLY
#define CAMERA_PROBE_ONLY 1
#endif

static const char *TAG = "esp32-cam Webserver";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
extern const unsigned char portal_html_start[] asm("_binary_portal_html_start");
extern const unsigned char portal_html_end[] asm("_binary_portal_html_end");

#define CONFIG_XCLK_FREQ 20000000 

static esp_err_t init_camera(void)
{
    camera_config_t camera_config = {
        .pin_pwdn  = -1,
        .pin_reset = -1,
        .pin_xclk = 10,
        .pin_sccb_sda = 40,
        .pin_sccb_scl = 39,

        .pin_d7 = 48,
        .pin_d6 = 11,
        .pin_d5 = 12,
        .pin_d4 = 14,
        .pin_d3 = 16,
        .pin_d2 = 18,
        .pin_d1 = 17,
        .pin_d0 = 15,
        .pin_vsync = 38,
        .pin_href = 47,
        .pin_pclk = 13,

        .xclk_freq_hz = CONFIG_XCLK_FREQ,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = CAMERA_FRAME_SIZE,

        .jpeg_quality = 10,
        .fb_count = 1,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY};//CAMERA_GRAB_LATEST. Sets when buffers should be filled
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        return err;
    }
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != NULL)
    {
        ESP_LOGI(TAG, "Camera sensor PID: 0x%04x", sensor->id.PID);
    }
    return ESP_OK;
}


esp_err_t jpg_stream_httpd_handler(httpd_req_t *req){
    char query[40];
    char size[12];
    framesize_t requested_size = CAMERA_FRAME_SIZE;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "size", size, sizeof(size)) == ESP_OK) {
        if (strcmp(size, "qvga") == 0) requested_size = FRAMESIZE_QVGA;
        else if (strcmp(size, "vga") == 0) requested_size = FRAMESIZE_VGA;
        else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Use size=qvga or size=vga");
            return ESP_FAIL;
        }
    }
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor == NULL || sensor->set_framesize(sensor, requested_size) != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera resolution change failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Stream resolution: %s", requested_size == FRAMESIZE_VGA ? "640x480" : "320x240");
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char part_buf[64];
    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        if(fb->format != PIXFORMAT_JPEG){
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG){
            free(_jpg_buf);
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        ESP_LOGI(TAG, "MJPG: %uKB %ums (%.1ffps)",
            (uint32_t)(_jpg_buf_len/1024),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time);
    }

    last_frame = 0;
    return res;
}

static esp_err_t portal_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)portal_html_start,
                           portal_html_end - portal_html_start);
}

static const httpd_uri_t portal_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = portal_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_get = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = jpg_stream_httpd_handler,
    .user_ctx = NULL};
httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t stream_httpd  = NULL;

    if (httpd_start(&stream_httpd , &config) == ESP_OK)
    {
        httpd_register_uri_handler(stream_httpd, &portal_uri);
        httpd_register_uri_handler(stream_httpd , &uri_get);
    }

    return stream_httpd;
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_main(void)
{
    esp_err_t err;
    
    printf("\n\nWeb Camera Serve Demo (Built " __DATE__ " " __TIME__ ")\n\n");
    err = init_camera();
    if (err != ESP_OK)
    {
        printf("Camera init failed: %s\n", esp_err_to_name(err));
        return;
    }

#if CAMERA_PROBE_ONLY
    ESP_LOGI(TAG, "Camera probe-only build; HaLow is not started");
    return;
#else
    /* Initialize and connect to Wi-Fi HaLow; blocks until connected. */
    app_wlan_init();
    app_wlan_start();
    
    esp_event_loop_create_default(); //httpd use event loop

    
    setup_server();
    ESP_LOGI(TAG, "ESP32 CAM Web Server is up and running\n");
    
    while(1) {
        app_wlan_arp_send();
        vTaskDelay(5000/portTICK_PERIOD_MS); //Tell the router that the device is online.
    }
#endif
}

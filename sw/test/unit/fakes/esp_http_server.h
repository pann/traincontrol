#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

typedef void *httpd_handle_t;

typedef enum {
    HTTP_GET = 0,
    HTTP_POST,
} httpd_method_t;

typedef struct httpd_req {
    httpd_handle_t handle;
    int method;
    char uri[512];
    size_t content_len;
} httpd_req_t;

typedef esp_err_t (*httpd_uri_handler_t)(httpd_req_t *r);

typedef struct {
    const char *uri;
    httpd_method_t method;
    httpd_uri_handler_t handler;
    void *user_ctx;
} httpd_uri_t;

typedef bool (*httpd_uri_match_func_t)(const char *uri_template,
                                        const char *uri_to_match,
                                        size_t len);

typedef struct {
    unsigned server_port;
    size_t max_uri_handlers;
    httpd_uri_match_func_t uri_match_fn;
} httpd_config_t;

#define HTTPD_DEFAULT_CONFIG() { \
    .server_port = 80,           \
    .max_uri_handlers = 8,       \
    .uri_match_fn = NULL,        \
}

#define HTTPD_RESP_USE_STRLEN (-1)
#define HTTPD_400_BAD_REQUEST "400 Bad Request"

extern httpd_uri_match_func_t httpd_uri_match_wildcard;

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config);
esp_err_t httpd_stop(httpd_handle_t handle);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri);
esp_err_t httpd_resp_set_type(httpd_req_t *req, const char *type);
esp_err_t httpd_resp_set_status(httpd_req_t *req, const char *status);
esp_err_t httpd_resp_set_hdr(httpd_req_t *req, const char *field, const char *value);
esp_err_t httpd_resp_send(httpd_req_t *req, const char *buf, int buf_len);
esp_err_t httpd_resp_send_err(httpd_req_t *req, const char *status, const char *msg);
int httpd_req_recv(httpd_req_t *req, char *buf, size_t buf_len);

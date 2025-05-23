#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include "esp_http_server.h"

// Max buffer size for JSON responses or request bodies
#define API_HANDLER_BUFFER_SIZE 1024 
// Max path length for SPIFFS files (used by static file handler)
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
// Scratch buffer size for file reading
#define SCRATCH_BUFSIZE  8192


// Handler function declarations
esp_err_t api_get_pokemon_list_handler(httpd_req_t *req);
esp_err_t api_get_pokemon_detail_handler(httpd_req_t *req);
esp_err_t api_post_trade_select_handler(httpd_req_t *req);
esp_err_t api_post_trade_start_handler(httpd_req_t *req);
esp_err_t api_get_trade_status_handler(httpd_req_t *req);

#endif // API_HANDLERS_H

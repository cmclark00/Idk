#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "api_handlers.h" // To link API handlers
#include <fcntl.h> // For open, close, read, write for SPIFFS

static const char *TAG = "web_server";

// --- Static File Serving Handler ---
static esp_err_t static_file_get_handler(httpd_req_t *req) {
    char filepath[FILE_PATH_MAX]; // Max length of SPIFFS file path
    const char *base_path = "/spiffs"; // SPIFFS mount point

    // Construct file path from URI
    if (strcmp(req->uri, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", base_path);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", base_path, req->uri);
    }

    // Check for directory traversal
    if (strstr(filepath, "..")) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid URI");
        return ESP_FAIL;
    }

    // Set Content-Type based on file extension
    const char *type = "text/plain";
    if (strstr(filepath, ".html")) type = "text/html";
    else if (strstr(filepath, ".css")) type = "text/css";
    else if (strstr(filepath, ".js")) type = "application/javascript";
    else if (strstr(filepath, ".png")) type = "image/png";
    else if (strstr(filepath, ".jpg")) type = "image/jpeg";
    else if (strstr(filepath, ".ico")) type = "image/x-icon";
    httpd_resp_set_type(req, type);

    // Open file
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Read and send file content in chunks
    char *chunk = malloc(SCRATCH_BUFSIZE);
    ssize_t read_bytes;
    do {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                free(chunk);
                ESP_LOGE(TAG, "File sending failed!");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);

    close(fd);
    free(chunk);
    httpd_resp_send_chunk(req, NULL, 0); // Finalize chunked response
    ESP_LOGI(TAG, "File sending complete: %s", filepath);
    return ESP_OK;
}


// --- HTTP Server Setup ---
httpd_handle_t start_web_server_httpd(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard; // Allow wildcard matching for static files

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        return NULL;
    }

    // --- Register API Handlers (from api_handlers.c) ---
    httpd_uri_t api_pokemon_list_uri = {
        .uri = "/api/pokemon",
        .method = HTTP_GET,
        .handler = api_get_pokemon_list_handler,
        .user_ctx = NULL // Or pointer to trade context / storage context if needed
    };
    httpd_register_uri_handler(server, &api_pokemon_list_uri);

    httpd_uri_t api_pokemon_detail_uri = {
        .uri = "/api/pokemon/*", // Wildcard for index
        .method = HTTP_GET,
        .handler = api_get_pokemon_detail_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_pokemon_detail_uri);
    
    httpd_uri_t api_trade_select_uri = {
        .uri = "/api/trade/select",
        .method = HTTP_POST,
        .handler = api_post_trade_select_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_trade_select_uri);

    httpd_uri_t api_trade_start_uri = {
        .uri = "/api/trade/start",
        .method = HTTP_POST,
        .handler = api_post_trade_start_handler,
        .user_ctx = NULL 
    };
    httpd_register_uri_handler(server, &api_trade_start_uri);

    httpd_uri_t api_trade_status_uri = {
        .uri = "/api/trade/status",
        .method = HTTP_GET,
        .handler = api_get_trade_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_trade_status_uri);


    // --- Register Static File Handler for all other GET requests ---
    // This should be registered last as a fallback for static content.
    httpd_uri_t static_files_uri = {
        .uri = "/*", // Catch-all for static files
        .method = HTTP_GET,
        .handler = static_file_get_handler,
        .user_ctx = NULL // Base path for SPIFFS could be passed here
    };
    httpd_register_uri_handler(server, &static_files_uri);

    return server;
}

void stop_web_server_httpd(httpd_handle_t server) {
    if (server) {
        httpd_stop(server);
    }
}

// --- SPIFFS Initialization ---
// (Typically called once at startup in main.c)
esp_err_t init_spiffs(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL, // Auto-find SPIFFS partition
      .max_files = 5,          // Max open files
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

// In main.c, you would call init_spiffs() and then start_web_server_httpd().
// The actual link cable communication would likely run in a separate FreeRTOS task,
// interacting with the TradeContext g_trade_context.
// API handlers would then read from/write to this shared g_trade_context.
// Mutexes/semaphores would be needed to protect access to g_trade_context.
```

**`api_handlers.h`**

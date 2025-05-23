#include "api_handlers.h"
#include "pokemon_storage.h"
#include "trade_protocol.h"
#include "esp_log.h"
#include "cJSON.h" // Using cJSON library for JSON manipulation (ESP-IDF component)
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For atoi

static const char *TAG = "api_handlers";

// External declaration of the global trade context (defined in trade_protocol.c or main.c)
extern TradeContext g_trade_context;
extern bool g_trade_context_initialized; // To check if init has been called

// --- Helper Functions ---
static void send_json_response(httpd_req_t *req, cJSON *root) {
    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        httpd_resp_send_500(req);
        ESP_LOGE(TAG, "Failed to print cJSON object");
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_string, strlen(json_string));
        free(json_string);
    }
    cJSON_Delete(root);
}

// --- API Handler Implementations ---

esp_err_t api_get_pokemon_list_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "/api/pokemon requested");
    cJSON *root_array = cJSON_CreateArray();
    if (root_array == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int count = get_stored_pokemon_count();
    for (int i = 0; i < count; i++) {
        PokemonTradeUnit *pkm = get_pokemon_from_storage(i);
        if (pkm && pkm->is_slot_occupied) {
            cJSON *pkm_json = cJSON_CreateObject();
            cJSON_AddNumberToObject(pkm_json, "storage_index", i);
            // Use species name as nickname if nickname is empty
            char nickname_display[NICKNAME_BUFFER_SIZE];
            if (pkm->nickname.name[0] == '\0') {
                snprintf(nickname_display, sizeof(nickname_display), "SPECIES_%d", pkm->main_data.species_id);
            } else {
                strncpy(nickname_display, pkm->nickname.name, sizeof(nickname_display) -1);
                nickname_display[sizeof(nickname_display)-1] = '\0';
            }
            cJSON_AddStringToObject(pkm_json, "nickname", nickname_display);
            cJSON_AddNumberToObject(pkm_json, "species_id", pkm->main_data.species_id);
            cJSON_AddNumberToObject(pkm_json, "level", pkm->main_data.level);
            cJSON_AddItemToArray(root_array, pkm_json);
        }
    }
    send_json_response(req, root_array);
    return ESP_OK;
}

esp_err_t api_get_pokemon_detail_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Request for %s", req->uri);
    // Extract index from URI, e.g., /api/pokemon/0 -> 0
    const char *index_str = req->uri + strlen("/api/pokemon/");
    int index = atoi(index_str);

    if (index < 0 || index >= MAX_POKEMON_STORAGE) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    PokemonTradeUnit *pkm = get_pokemon_from_storage(index);
    if (!pkm || !pkm->is_slot_occupied) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "storage_index", index);
    
    cJSON *pkm_data_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(pkm_data_json, "species_id", pkm->main_data.species_id);
    cJSON_AddNumberToObject(pkm_data_json, "current_hp", pkm->main_data.current_hp);
    cJSON_AddNumberToObject(pkm_data_json, "level_box", pkm->main_data.level_box);
    cJSON_AddNumberToObject(pkm_data_json, "status_condition", pkm->main_data.status_condition);
    cJSON_AddNumberToObject(pkm_data_json, "type1", pkm->main_data.type1);
    cJSON_AddNumberToObject(pkm_data_json, "type2", pkm->main_data.type2);
    cJSON_AddNumberToObject(pkm_data_json, "catch_rate_or_held_item", pkm->main_data.catch_rate_or_held_item);
    cJSON_AddNumberToObject(pkm_data_json, "move1_id", pkm->main_data.move1_id);
    cJSON_AddNumberToObject(pkm_data_json, "move2_id", pkm->main_data.move2_id);
    cJSON_AddNumberToObject(pkm_data_json, "move3_id", pkm->main_data.move3_id);
    cJSON_AddNumberToObject(pkm_data_json, "move4_id", pkm->main_data.move4_id);
    cJSON_AddNumberToObject(pkm_data_json, "original_trainer_id", pkm->main_data.original_trainer_id);
    // Experience: Gen 1 is 3 bytes. cJSON doesn't have a direct byte array, send as array of numbers or hex string.
    cJSON *exp_array = cJSON_CreateArray();
    cJSON_AddItemToArray(exp_array, cJSON_CreateNumber(pkm->main_data.experience[0]));
    cJSON_AddItemToArray(exp_array, cJSON_CreateNumber(pkm->main_data.experience[1]));
    cJSON_AddItemToArray(exp_array, cJSON_CreateNumber(pkm->main_data.experience[2]));
    cJSON_AddItemToObject(pkm_data_json, "experience", exp_array);
    cJSON_AddNumberToObject(pkm_data_json, "hp_ev", pkm->main_data.hp_ev);
    cJSON_AddNumberToObject(pkm_data_json, "attack_ev", pkm->main_data.attack_ev);
    cJSON_AddNumberToObject(pkm_data_json, "defense_ev", pkm->main_data.defense_ev);
    cJSON_AddNumberToObject(pkm_data_json, "speed_ev", pkm->main_data.speed_ev);
    cJSON_AddNumberToObject(pkm_data_json, "special_ev", pkm->main_data.special_ev);
    cJSON_AddNumberToObject(pkm_data_json, "iv_data", pkm->main_data.iv_data);
    cJSON_AddNumberToObject(pkm_data_json, "move1_pp", pkm->main_data.move1_pp);
    cJSON_AddNumberToObject(pkm_data_json, "move2_pp", pkm->main_data.move2_pp);
    cJSON_AddNumberToObject(pkm_data_json, "move3_pp", pkm->main_data.move3_pp);
    cJSON_AddNumberToObject(pkm_data_json, "move4_pp", pkm->main_data.move4_pp);
    cJSON_AddNumberToObject(pkm_data_json, "level", pkm->main_data.level);
    cJSON_AddNumberToObject(pkm_data_json, "max_hp", pkm->main_data.max_hp);
    cJSON_AddNumberToObject(pkm_data_json, "attack", pkm->main_data.attack);
    cJSON_AddNumberToObject(pkm_data_json, "defense", pkm->main_data.defense);
    cJSON_AddNumberToObject(pkm_data_json, "speed", pkm->main_data.speed);
    cJSON_AddNumberToObject(pkm_data_json, "special", pkm->main_data.special);
    cJSON_AddItemToObject(root, "pokemon_data", pkm_data_json);

    cJSON_AddStringToObject(root, "ot_name", pkm->ot_name.name);
    cJSON_AddStringToObject(root, "nickname", pkm->nickname.name);

    send_json_response(req, root);
    return ESP_OK;
}

esp_err_t api_post_trade_select_handler(httpd_req_t *req) {
    char buf[100]; // Buffer for request body
    int ret, remaining = req->content_len;
    cJSON *json_body = NULL;
    cJSON *json_status = cJSON_CreateObject();

    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_413_PAYLOAD_TOO_LARGE, "Payload too large");
        cJSON_Delete(json_status);
        return ESP_FAIL;
    }

    // Read request body
    int received = httpd_req_recv(req, buf, remaining);
    if (received <= 0) { // Error or empty body
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        cJSON_Delete(json_status);
        return ESP_FAIL;
    }
    buf[received] = '\0'; // Null-terminate

    // Parse JSON body
    json_body = cJSON_Parse(buf);
    if (json_body == NULL) {
        cJSON_AddStringToObject(json_status, "status", "error");
        cJSON_AddStringToObject(json_status, "message", "Invalid JSON format.");
        send_json_response(req, json_status);
        httpd_resp_set_status(req, HTTPD_400_BAD_REQUEST);
        return ESP_FAIL;
    }

    const cJSON *storage_index_json = cJSON_GetObjectItemCaseSensitive(json_body, "storage_index");
    if (!cJSON_IsNumber(storage_index_json) || storage_index_json->valueint < 0 || storage_index_json->valueint >= MAX_POKEMON_STORAGE) {
        cJSON_AddStringToObject(json_status, "status", "error");
        cJSON_AddStringToObject(json_status, "message", "Invalid or missing 'storage_index'.");
        send_json_response(req, json_status);
        httpd_resp_set_status(req, HTTPD_400_BAD_REQUEST);
        cJSON_Delete(json_body);
        return ESP_FAIL;
    }
    
    uint8_t selected_idx = (uint8_t)storage_index_json->valueint;

    if (!g_trade_context_initialized) { // Initialize global context if not done yet
        // Default to master, actual Pokemon selection will be confirmed by this call
        trade_init(&g_trade_context, true, -1); 
        g_trade_context_initialized = true;
    }
    
    if (trade_api_select_pokemon_to_offer(&g_trade_context, selected_idx)) {
        cJSON_AddStringToObject(json_status, "status", "success");
        char msg_buf[100];
        snprintf(msg_buf, sizeof(msg_buf), "Pokemon at index %d selected for next trade.", selected_idx);
        cJSON_AddStringToObject(json_status, "message", msg_buf);
        
        cJSON* pkm_info = cJSON_CreateObject();
        cJSON_AddNumberToObject(pkm_info, "storage_index", g_trade_context.player_pokemon_index);
        cJSON_AddStringToObject(pkm_info, "nickname", g_trade_context.pokemon_to_send.nickname.name);
        cJSON_AddNumberToObject(pkm_info, "species_id", g_trade_context.pokemon_to_send.main_data.species_id);
        cJSON_AddItemToObject(json_status, "selected_pokemon_info", pkm_info);
        
        send_json_response(req, json_status);
    } else {
        cJSON_AddStringToObject(json_status, "status", "error");
        cJSON_AddStringToObject(json_status, "message", "Failed to select Pokemon (not found or invalid).");
        send_json_response(req, json_status);
        httpd_resp_set_status(req, HTTPD_404_NOT_FOUND);
    }

    cJSON_Delete(json_body);
    return ESP_OK;
}


esp_err_t api_post_trade_start_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "/api/trade/start requested");
    cJSON *json_status = cJSON_CreateObject();

    if (!g_trade_context_initialized || !g_trade_context.local_pokemon_selected) {
        cJSON_AddStringToObject(json_status, "status", "error");
        cJSON_AddStringToObject(json_status, "message", "No Pokemon selected for trade. Please select a Pokemon first via /api/trade/select.");
        send_json_response(req, json_status);
        httpd_resp_set_status(req, HTTPD_400_BAD_REQUEST);
        return ESP_FAIL;
    }
    
    // Assuming g_trade_context is already initialized and Pokemon selected.
    // The role (master/slave) might be fixed or configurable. Defaulting to master for ESP32 side.
    trade_api_start_session(&g_trade_context, true); // true for master

    if (g_trade_context.current_state == TRADE_STATE_ERROR) { // If start_session itself detected an issue
        cJSON_AddStringToObject(json_status, "status", "error_starting_trade");
        cJSON_AddStringToObject(json_status, "message", "Failed to start trade session (e.g. no Pokemon available internally).");
    } else {
        cJSON_AddStringToObject(json_status, "status", "trade_initiated");
        cJSON_AddStringToObject(json_status, "message", "Device is now attempting to connect for trading. Monitor status via /api/trade/status.");
    }
    send_json_response(req, json_status);
    return ESP_OK;
}

esp_err_t api_get_trade_status_handler(httpd_req_t *req) {
    // ESP_LOGV(TAG, "/api/trade/status requested"); // Verbose logging for status
    if (!g_trade_context_initialized) { // Initialize if first call (e.g. page load)
        // Default to master, no Pokemon selected yet by API.
        // This ensures g_trade_context exists for trade_api_get_status_json.
        trade_init(&g_trade_context, true, -1); 
        g_trade_context_initialized = true;
    }

    char status_buf[API_HANDLER_BUFFER_SIZE]; // Ensure this is large enough for the JSON
    int len = trade_api_get_status_json(&g_trade_context, status_buf, sizeof(status_buf));

    if (len < 0) {
        httpd_resp_send_500(req);
        ESP_LOGE(TAG, "Failed to generate trade status JSON.");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, status_buf, len);
    return ESP_OK;
}

// In main.c, you would typically initialize:
// 1. NVS (Non-Volatile Storage)
// 2. SPIFFS (for static files)
// 3. Wi-Fi (Station or AP mode)
// 4. Initialize Pokemon Storage (e.g., load from NVS or use defaults)
//    initialize_pokemon_storage();
//    // Potentially add some dummy Pokemon for testing
//    PokemonTradeUnit test_pkm;
//    test_pkm.main_data.species_id = 25; // Pikachu
//    strcpy(test_pkm.nickname.name, "PIKA_ESP32");
//    strcpy(test_pkm.ot_name.name, "ESP_MASTER");
//    test_pkm.main_data.level = 50;
//    test_pkm.is_slot_occupied = 1;
//    add_pokemon_to_storage(&test_pkm);
// 5. Initialize Trade Context (partially, or fully on first API call)
//    g_trade_context_initialized = false; // Set to false, let API calls init as needed
// 6. Start Web Server (which registers these API handlers)
//    start_web_server_httpd();
// 7. Start Link Cable Communication Task (FreeRTOS task)
//    This task would periodically call something like:
//    uint8_t byte_from_gb = link_cable_receive_byte_if_available(); // Non-blocking
//    if (byte_from_gb_was_received) {
//        uint8_t byte_to_gb = trade_api_process_incoming_byte(&g_trade_context, byte_from_gb);
//        link_cable_send_byte(byte_to_gb);
//    } else {
//        // If master and needs to send proactively
//        uint8_t proactive_byte = trade_api_get_outgoing_byte(&g_trade_context);
//        if (proactive_byte_needs_sending) link_cable_send_byte(proactive_byte);
//    }
//    check_trade_timeout(&g_trade_context);
//    vTaskDelay(pdMS_TO_TICKS(10)); // Example delay
```

**`main.c` (Conceptual Snippet)**

```c
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "protocol_examples_common.h" // For Wi-Fi connect example
#include "web_server.h"          // Contains start_web_server_httpd and init_spiffs
#include "pokemon_storage.h"
#include "trade_protocol.h"      // For g_trade_context
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Global context declared in trade_protocol.c or here
// If in trade_protocol.c, use: extern TradeContext g_trade_context;
// If here: TradeContext g_trade_context;
// extern bool g_trade_context_initialized; // Also from trade_protocol.c

// --- Link Cable Task (Simplified Example) ---
// This needs actual hardware GPIO interaction for Game Boy link cable
// For now, it's a placeholder showing how it interacts with trade_protocol
#define LINK_CABLE_CLOCK_PIN GPIO_NUM_XX // Define your GPIOs
#define LINK_CABLE_MOSI_PIN  GPIO_NUM_XX
#define LINK_CABLE_MISO_PIN  GPIO_NUM_XX

// Simulated hardware functions
static uint8_t s_current_byte_to_send_gb = PKMN_BLANK;
static bool s_byte_received_from_gb_flag = false;
static uint8_t s_last_received_byte_from_gb = PKMN_BLANK;

// Call this from an interrupt or polling routine for SCLK
void IRAM_ATTR link_cable_handle_bit_transfer() {
    // Complex bit-banging logic here based on SCLK, MOSI, MISO
    // When a full byte is received:
    // s_last_received_byte_from_gb = received_byte_val;
    // s_byte_received_from_gb_flag = true;
    // When a full byte needs to be sent, shift out s_current_byte_to_send_gb
}

void link_cable_task(void *pvParameters) {
    esp_rom_gpio_pad_select_gpio(LINK_CABLE_MISO_PIN);
    gpio_set_direction(LINK_CABLE_MISO_PIN, GPIO_MODE_INPUT);
    // ... setup other pins ...

    while (1) {
        if (g_trade_context_initialized) { // Only run if context is ready
            if (s_byte_received_from_gb_flag) {
                s_byte_received_from_gb_flag = false;
                s_current_byte_to_send_gb = trade_api_process_incoming_byte(&g_trade_context, s_last_received_byte_from_gb);
            } else {
                // If master and needs to send proactively (e.g., initial PKMN_MASTER)
                // This logic needs refinement: trade_api_get_outgoing_byte might be better.
                if (g_trade_context.is_master_role && 
                    (g_trade_context.current_state == TRADE_STATE_INIT_MASTER || 
                     g_trade_context.current_state == TRADE_STATE_TC_READY_TO_GO)) { // Example states
                    // s_current_byte_to_send_gb = trade_api_get_outgoing_byte(&g_trade_context);
                    // This proactive send needs careful handling to not conflict with reactive sends.
                    // For simplicity, the Arduino example relied on the GB clocking out data.
                }
            }
            check_trade_timeout(&g_trade_context); // Check for timeouts
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // Small delay for the task
    }
}


void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SPIFFS
    ESP_ERROR_CHECK(init_spiffs()); // From web_server.c

    // Initialize Pokémon Storage
    initialize_pokemon_storage();
    // Add some dummy Pokemon for testing
    PokemonTradeUnit test_pkm;
    memset(&test_pkm, 0, sizeof(PokemonTradeUnit));
    test_pkm.main_data.species_id = 25; // Pikachu
    strncpy(test_pkm.nickname.name, "PIKA_ESP32", NICKNAME_BUFFER_SIZE - 1);
    strncpy(test_pkm.ot_name.name, "ESP_MASTER", OT_NAME_BUFFER_SIZE - 1);
    test_pkm.main_data.level = 50;
    test_pkm.is_slot_occupied = 1;
    add_pokemon_to_storage(&test_pkm);

    PokemonTradeUnit test_pkm2;
    memset(&test_pkm2, 0, sizeof(PokemonTradeUnit));
    test_pkm2.main_data.species_id = 1; // Bulbasaur
    strncpy(test_pkm2.nickname.name, "BULBA", NICKNAME_BUFFER_SIZE - 1);
    strncpy(test_pkm2.ot_name.name, "ASH", OT_NAME_BUFFER_SIZE - 1);
    test_pkm2.main_data.level = 5;
    test_pkm2.is_slot_occupied = 1;
    add_pokemon_to_storage(&test_pkm2);


    // Initialize Wi-Fi (using helper from protocol_examples_common)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect()); // Connects to Wi-Fi configured in Kconfig

    // Start the web server
    start_web_server_httpd(); // From web_server.c

    // Initialize trade context (can be lazy-initialized by first API call too)
    // trade_init(&g_trade_context, true, -1); // Initialize as master, no Pokemon selected by default
    // g_trade_context_initialized = true; 
    // Better to let API handlers set g_trade_context_initialized = true

    // Create FreeRTOS task for link cable communication
    // xTaskCreate(&link_cable_task, "link_cable_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "ESP32 Pokemon Trader Initialized.");
}

```

**Brief Description of Project Structure (ESP-IDF):**

*   **`main/` directory:**
    *   `main.c`: Contains `app_main()`, initializes NVS, SPIFFS, Wi-Fi, Pokémon storage, and starts the web server and the link cable communication FreeRTOS task.
    *   `api_handlers.c`/`.h`: Implement the functions that handle HTTP requests for the defined API endpoints. They parse requests, call functions from `pokemon_storage.c` and `trade_protocol.c`, and format JSON responses using `cJSON`.
    *   `web_server.c`/`.h`: Contains the ESP32 HTTP server setup (`esp_http_server`), registration of URI handlers (for both API and static files), and the logic for serving static files from SPIFFS. Includes `init_spiffs()`.
    *   `pokemon_storage.c`/`.h`: Code from previous step for managing Pokémon in memory.
    *   `trade_protocol.c`/`.h`: Code from previous step for the trade state machine and protocol logic. Contains the global `g_trade_context` and new `trade_api_...` functions.
    *   `CMakeLists.txt`: Standard ESP-IDF build file for the `main` component, listing source files and dependencies (like `esp_http_server`, `esp_spiffs`, `cjson`).
*   **`spiffs_image/` directory:** (At the project root, next to `main/`)
    *   Contains `index.html`, `style.css`, `script.js`. These files are flashed to the SPIFFS partition on the ESP32. The build system needs to be configured to create a SPIFFS image from this directory (e.g., using `spiffsgen.py`).
*   **`sdkconfig`:** ESP-IDF project configuration file. Used to set Wi-Fi credentials (if using STA mode), partition table settings, component configurations, etc.
*   **`partitions.csv`:** Defines the flash partition layout, including a partition for SPIFFS (e.g., type `data`, subtype `spiffs`).

**New/Modified Interface Functions (Summary):**

*   **Added to `trade_protocol.h/c`:**
    *   `trade_api_select_pokemon_to_offer(TradeContext* context, uint8_t storage_idx)`: Sets the Pokémon to be offered.
    *   `trade_api_start_session(TradeContext* context, bool is_master_role_requested)`: Initiates a trade session, setting up the context and initial state.
    *   `trade_api_get_status_json(TradeContext* context, char* buffer, size_t buffer_len)`: Generates a JSON string representing the current trade status.
    *   `trade_api_process_incoming_byte(TradeContext* context, uint8_t received_byte)`: Wrapper for `trade_process_byte` for use by the link cable task.
    *   `trade_api_get_outgoing_byte(TradeContext* context)`: Helper for the link cable task to get proactive bytes to send (e.g., initial master signal).
    *   `trade_api_get_received_pokemon_storage_index(TradeContext* context)`: Placeholder for retrieving the storage index of a newly received Pokémon.
    *   `trade_state_to_string(TradeState state)` and `trade_state_message(TradeState state)`: Helper functions for generating human-readable status in JSON.
*   **Global Variables (Conceptual, likely in `trade_protocol.c` or `main.c`):**
    *   `TradeContext g_trade_context;`: The single, global instance of the trade state.
    *   `bool g_trade_context_initialized;`: Flag to ensure `trade_init` is called appropriately.

This setup provides a functional backend for the web UI, allowing it to interact with the Pokémon storage and trade logic running on the ESP32. The actual link cable bit-banging logic in `link_cable_task` would be highly hardware-dependent and require careful timing.

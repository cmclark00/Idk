#include "serial_protocol.h"
#include "pico/stdlib.h" // For stdio_init_all, getchar_timeout_us, etc.
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For atoi

#include "pokemon_storage.h"
#include "trade_protocol.h"

// --- External Global Variables (defined in trade_protocol.c or main.c) ---
// Ensure these are appropriately defined and accessible.
// For this example, we assume they are defined in trade_protocol.c or main.c
extern TradeContext g_trade_context;
extern bool g_trade_context_initialized;


// --- Static Variables for Command Processing ---
static char cmd_buffer[SERIAL_CMD_BUFFER_SIZE];
static uint32_t cmd_buffer_idx = 0;

// --- Handler Function Declarations ---
static void handle_list_pokemon(void);
static void handle_select_pokemon(const char* args);
static void handle_initiate_trade(const char* args); // Args for optional role
static void handle_get_status(void);
static void handle_cancel_trade(void);
static void handle_unknown_command(const char* command);
static void process_command(const char* command_line);


// --- Public Function Implementations ---

/**
 * @brief Initializes the serial command processor and USB CDC.
 */
void serial_protocol_init(void) {
    // stdio_init_all() is called by the RP2040 SDK runtime before main()
    // or can be called explicitly in main.c.
    // If not called by default in your setup, ensure it's called once.
    // For this module, we assume it's already been called or will be.
    // printf("Serial Protocol Initialized (USB CDC should be active).\n");
    memset(cmd_buffer, 0, SERIAL_CMD_BUFFER_SIZE);
    cmd_buffer_idx = 0;
}

/**
 * @brief Processes a single character received from the serial input.
 * Buffers characters until a newline is detected, then parses and handles the command.
 */
void serial_protocol_process_char(char received_char) {
    if (received_char == '\n' || received_char == '\r') { // End of command
        if (cmd_buffer_idx > 0) { // If there's a command in the buffer
            cmd_buffer[cmd_buffer_idx] = '\0'; // Null-terminate
            process_command(cmd_buffer);
            cmd_buffer_idx = 0; // Reset buffer index for next command
            memset(cmd_buffer, 0, SERIAL_CMD_BUFFER_SIZE); // Clear buffer
        }
    } else if (cmd_buffer_idx < (SERIAL_CMD_BUFFER_SIZE - 1)) {
        cmd_buffer[cmd_buffer_idx++] = received_char;
    } else {
        // Buffer overflow, reset (or send error)
        printf("ERROR Command buffer overflow\n");
        cmd_buffer_idx = 0;
        memset(cmd_buffer, 0, SERIAL_CMD_BUFFER_SIZE);
    }
}

/**
 * @brief Alternative function to be called in a loop to read from stdio.
 * This is useful if not using interrupts for character reception.
 */
void serial_protocol_poll_and_process(void) {
    int c = getchar_timeout_us(0); // Non-blocking read
    if (c != PICO_ERROR_TIMEOUT && c != EOF) {
        serial_protocol_process_char((char)c);
    }
}


// --- Static Helper and Command Processing Functions ---

/**
 * @brief Parses and dispatches a complete command line.
 */
static void process_command(const char* command_line) {
    printf("DEBUG: Received command line: '%s'\n", command_line); // Echo for debug

    char command[32]; // Buffer for the command part
    char args[SERIAL_CMD_BUFFER_SIZE - 32]; // Buffer for arguments

    // Simple parsing: command is the first word, rest are args
    int n = sscanf(command_line, "%31s %[^\n]", command, args);

    if (n < 1) { // No command found
        return;
    }
    if (n == 1) { // Command with no arguments
        args[0] = '\0';
    }

    if (strcmp(command, "LIST_POKEMON") == 0) {
        handle_list_pokemon();
    } else if (strcmp(command, "SELECT_POKEMON") == 0) {
        handle_select_pokemon(args);
    } else if (strcmp(command, "INITIATE_TRADE") == 0) {
        handle_initiate_trade(args);
    } else if (strcmp(command, "GET_STATUS") == 0) {
        handle_get_status();
    } else if (strcmp(command, "CANCEL_TRADE") == 0) {
        handle_cancel_trade();
    } else {
        handle_unknown_command(command);
    }
}

static void handle_list_pokemon(void) {
    printf("POKEMON_LIST_START\n");
    int count = get_stored_pokemon_count();
    if (count == 0) {
        printf("INFO No Pokemon in storage.\n");
    } else {
        for (int i = 0; i < MAX_POKEMON_STORAGE; ++i) { // Iterate up to MAX_STORAGE to respect sparse indices
            PokemonTradeUnit *pkm = get_pokemon_from_storage(i);
            if (pkm && pkm->is_slot_occupied) {
                char name_to_print[NICKNAME_BUFFER_SIZE];
                if (pkm->nickname.name[0] == '\0') {
                    // Create a species name string if nickname is empty
                    snprintf(name_to_print, sizeof(name_to_print), "SPECIES_ID_%u", pkm->main_data.species_id);
                } else {
                    strncpy(name_to_print, pkm->nickname.name, NICKNAME_BUFFER_SIZE -1);
                    name_to_print[NICKNAME_BUFFER_SIZE-1] = '\0';
                }
                // Format: POKEMON <index> <nickname_or_species_placeholder> <species_id>
                printf("POKEMON %d %s %u\n", i, name_to_print, pkm->main_data.species_id);
            }
        }
    }
    printf("POKEMON_LIST_END\n");
}

static void handle_select_pokemon(const char* args) {
    int index;
    if (sscanf(args, "%d", &index) == 1) {
        if (index >= 0 && index < MAX_POKEMON_STORAGE) {
            // Initialize trade context if this is the first relevant command
            if (!g_trade_context_initialized) {
                trade_init(&g_trade_context, true, -1); // Default to master, no Pokemon pre-selected by init
                g_trade_context_initialized = true;
            }

            if (trade_serial_select_pokemon_to_offer(&g_trade_context, (uint8_t)index)) {
                printf("ACK_SELECT %d\n", index);
            } else {
                printf("ERROR Pokemon not found or invalid at index %d\n", index);
            }
        } else {
            printf("ERROR Invalid index %d. Must be between 0 and %d.\n", index, MAX_POKEMON_STORAGE - 1);
        }
    } else {
        printf("ERROR Missing or invalid index for SELECT_POKEMON command.\nFORMAT: SELECT_POKEMON <index>\n");
    }
}

static void handle_initiate_trade(const char* args) {
    bool is_master = true; // Default to master
    // Optional: allow role selection via args, e.g., "INITIATE_TRADE MASTER" or "INITIATE_TRADE SLAVE"
    if (args && args[0] != '\0') {
        if (strcmp(args, "SLAVE") == 0) {
            is_master = false;
        } else if (strcmp(args, "MASTER") != 0) {
            printf("INFO Unknown role '%s'. Defaulting to MASTER. Use MASTER or SLAVE.\n", args);
        }
    }

    // Initialize trade context if this is the first relevant command, ensuring a Pokemon is selected
    if (!g_trade_context_initialized) {
        trade_init(&g_trade_context, is_master, -1); // -1 initially, start_session will try to pick first if none selected
        g_trade_context_initialized = true;
    } else {
        // If already initialized, ensure the master/slave role is updated if it changed
        // and re-initialize with the currently selected Pokemon (or let start_session pick)
        // A full trade_init might be too disruptive if a Pokemon was already selected.
        // For now, trade_serial_start_session handles the core re-init logic.
        g_trade_context.is_master_role = is_master; // Update role if context already exists
    }

    if (!g_trade_context.local_pokemon_selected) {
         printf("INFO No Pokemon selected. Attempting to use first available.\n");
         // trade_serial_start_session will try to auto-select if none is chosen
    }
    
    trade_serial_start_session(&g_trade_context, is_master);

    if (g_trade_context.current_state == TRADE_STATE_ERROR) {
        printf("ERROR Could not initiate trade (e.g., no Pokemon available or other init error).\n");
    } else {
        printf("ACK_INITIATE %s\n", is_master ? "MASTER" : "SLAVE");
    }
}

static void handle_get_status(void) {
    char state_str_buf[64];
    char msg_str_buf[128];

    if (!g_trade_context_initialized) {
        // Provide a default "not initialized" status
        snprintf(state_str_buf, sizeof(state_str_buf), "%s", trade_state_to_string(TRADE_STATE_IDLE));
        snprintf(msg_str_buf, sizeof(msg_str_buf), "%s (Context not yet initialized by a trade command)", trade_state_message(TRADE_STATE_IDLE));
    } else {
        trade_serial_get_status_strings(&g_trade_context, 
                                        state_str_buf, sizeof(state_str_buf),
                                        msg_str_buf, sizeof(msg_str_buf));
    }
    printf("STATUS %s %s\n", state_str_buf, msg_str_buf);
}

static void handle_cancel_trade(void) {
    if (!g_trade_context_initialized) {
        printf("INFO Trade context not initialized. Nothing to cancel.\n");
        return;
    }
    trade_serial_cancel_trade(&g_trade_context);
    printf("ACK_CANCEL Trade cancelled or reset.\n");
}

static void handle_unknown_command(const char* command) {
    printf("ERROR Unknown command: %s\n", command);
}

/*
Example usage in main.c:

#include "pico/stdlib.h"
#include "serial_protocol.h"
// ... other includes ...

// In main():
// stdio_init_all(); // Initializes USB CDC for printf/getchar
// serial_protocol_init(); // Call this once
//
// // In the main loop:
// while (true) {
//     serial_protocol_poll_and_process(); // Call this repeatedly
//     // ... other tasks ...
//     tight_loop_contents(); // Or sleep, etc.
// }
*/

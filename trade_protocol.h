#ifndef TRADE_PROTOCOL_H
#define TRADE_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "pokemon_storage.h" // Assumes this file exists and defines PokemonTradeUnit, etc.

// --- Constants from pokemon.h analysis (Arduino project) ---
// These should be reviewed and potentially renamed/organized for clarity in a new C project
#define PKMN_BLANK                                              0x00
#define PKMN_MASTER                                             0x01
#define PKMN_SLAVE                                              0x02
#define PKMN_CONNECTED                                  0x60 // General "connected" or "action" byte
#define PKMN_WAIT                                               0x7F // General "wait" signal, though usage might vary

// Menu selections that lead to trade/colosseum (from pokemon.h, mapped to Game Boy's internal values)
#define MENU_ITEM_1_SELECTED                            0xD4 // Typically Trade
#define MENU_ITEM_2_SELECTED                            0xD5 // Typically Colosseum
#define MENU_ITEM_3_SELECTED                            0xD6 // Typically Cancel/Break Link

#define PKMN_TRADE_CENTRE                               MENU_ITEM_1_SELECTED
#define PKMN_COLOSSEUM                                  MENU_ITEM_2_SELECTED
#define PKMN_BREAK_LINK                                 MENU_ITEM_3_SELECTED

#define TRADE_CENTRE_WAIT                               0xFD // Sync byte used extensively in the trade sequence

// Player data block and patch data sizes (inferred from gameboy.ino)
#define PLAYER_DATA_BLOCK_SIZE                          512
#define PATCH_DATA_BLOCK_SIZE                           197

// Trade confirmation/selection bytes
#define TRADE_ACTION_CONFIRM_SELECTION_PREFIX           0x60 // Base for 0x60-0x65 (pokemon selection)
                                                             // Also used for "YES" to final confirmation
#define TRADE_ACTION_CANCEL_SELECTION                   0x6F // Cancel at Pokemon selection screen
#define TRADE_ACTION_CANCEL_FINAL                       0x61 // "NO" to final trade confirmation

// --- State Enum ---
typedef enum {
    TRADE_STATE_IDLE,                     // Not connected or doing anything

    // Connection & Role Negotiation
    TRADE_STATE_INIT_MASTER,              // Master trying to establish connection
    TRADE_STATE_INIT_SLAVE,               // Slave trying to establish connection
    TRADE_STATE_CONNECTION_ESTABLISHED,   // Basic link established, waiting for mode (Trade/Colosseum)

    // Trade Center Specific States
    TRADE_STATE_TC_INIT,                  // Entered Trade Center, initial 0x00 exchange
    TRADE_STATE_TC_READY_TO_GO,           // Sent/Received initial 0x00, waiting for 0xFD
    TRADE_STATE_TC_SEEN_FIRST_WAIT,       // Exchanged first 0xFD, slave sends first random byte
    TRADE_STATE_TC_EXCHANGING_RANDOM_DATA,// Exchanging random data block (master and slave)
    TRADE_STATE_TC_WAITING_FOR_MAIN_DATA, // Random data done, exchanged 0xFD, waiting for first byte of main data
    
    TRADE_STATE_TC_EXCHANGING_MAIN_DATA,  // Exchanging the main PLAYER_DATA_BLOCK_SIZE data
    TRADE_STATE_TC_EXCHANGING_PATCH_DATA, // Exchanging the PATCH_DATA_BLOCK_SIZE data

    // Post-Data Exchange States (Selection & Confirmation)
    TRADE_STATE_TC_AWAITING_SELECTION,    // Both sent data, waiting for local/remote Pokemon selection (0x60-0x65 or 0x6F)
                                          // Or waiting for 0x00 from master to proceed to final confirm screen
    TRADE_STATE_TC_AWAITING_CONFIRMATION, // Pokemon selected by both, on final "TRADE?" screen (waiting 0x60 or 0x61)
    
    TRADE_STATE_TC_TRADE_CONFIRMED,       // Trade confirmed (0x60 received), local "YES" also sent.
                                          // Pokemon data internally swapped.
    TRADE_STATE_TC_TRADE_CANCELLED_POST_SELECTION, // Remote cancelled after selection (e.g. 0x6F) or local cancel
    TRADE_STATE_TC_TRADE_CANCELLED_FINAL, // Remote chose "NO" (0x61) or local cancel
    
    TRADE_STATE_TC_COMPLETE_WAIT_ACK,     // Trade logic done, sent data to game, waiting for 0x00 from game
    TRADE_STATE_TC_COMPLETE,              // Session done, ready for another or exit

    TRADE_STATE_ERROR                     // An error occurred
} TradeState;

// --- Global/Context Structure ---
typedef struct {
    TradeState current_state;
    bool is_master_role;         // Is this device acting as master?
    uint8_t player_pokemon_index; // Index in local storage of Pokemon to be traded
    PokemonTradeUnit pokemon_to_send; // Buffer for the Pokemon being sent (copied from storage)
    PokemonTradeUnit pokemon_received; // Buffer for the Pokemon being received

    uint8_t send_buffer[PLAYER_DATA_BLOCK_SIZE > PATCH_DATA_BLOCK_SIZE ? PLAYER_DATA_BLOCK_SIZE : PATCH_DATA_BLOCK_SIZE];
    uint8_t receive_buffer[PLAYER_DATA_BLOCK_SIZE > PATCH_DATA_BLOCK_SIZE ? PLAYER_DATA_BLOCK_SIZE : PATCH_DATA_BLOCK_SIZE];
    
    uint16_t current_block_index;   // Current byte index within the current block (random, main, or patch)
    uint16_t total_block_size;      // Total size of the current block being transferred

    uint8_t random_data_exchange_count; // How many random bytes exchanged
    #define RANDOM_DATA_BLOCK_SIZE_EXPECTED 3 // Example size, actual size from game behavior research needed

    uint8_t last_byte_sent;
    uint8_t last_byte_received;

    uint8_t remote_selected_pokemon_slot; // 0-5 for which Pokemon remote player selected
    bool local_pokemon_selected;
    bool remote_pokemon_selected;
    bool local_trade_confirmed; // Local player confirmed "YES" on final screen
    bool remote_trade_confirmed; // Remote player confirmed "YES"

    uint32_t last_comm_time_ms;     // Timestamp of the last communication
    uint32_t timeout_ms;            // Timeout duration

    // Pointers to actual storage units for final swap
    // These would be set after a trade is fully confirmed.
    PokemonTradeUnit* local_pokemon_storage_ptr;
    // The received Pokemon is in pokemon_received, to be added to storage.

} TradeContext;

// --- Function Declarations ---

void trade_init(TradeContext* context, bool is_master, uint8_t local_pokemon_storage_idx);
void trade_process_byte(TradeContext* context, uint8_t received_byte, uint8_t* byte_to_send_next);
uint8_t trade_get_next_byte_to_send(TradeContext* context); // Internal helper, might be merged into process_byte
void check_trade_timeout(TradeContext* context); // Placeholder

// Helper function to prepare the main data block to be sent
void trade_prepare_player_data_block(TradeContext* context);
// Helper function to process the received main data block
void trade_process_received_player_data(TradeContext* context);
// Helper function to prepare the patch data block
void trade_prepare_patch_data_block(TradeContext* context);
// Helper function to process the received patch data
void trade_process_received_patch_data(TradeContext* context);


// Placeholder for actual hardware/link functions
// These would be implemented elsewhere, specific to the hardware (Arduino, etc.)
// uint8_t link_cable_send_receive(uint8_t byte_to_send);
// bool link_cable_has_byte(uint8_t* received_byte);
// uint32_t get_current_time_ms();


#endif // TRADE_PROTOCOL_H

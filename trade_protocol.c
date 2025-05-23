#include "trade_protocol.h"
#include <string.h> // For memcpy, memset
#include <stdio.h>  // For debug printf, remove in production

// --- Static Helper Function Declarations ---
static void trade_reset_context_for_new_session(TradeContext* context);
static void trade_load_pokemon_to_send_buffer(TradeContext* context, uint8_t local_pokemon_storage_idx);
static uint8_t get_byte_from_pokemon_data(TradeContext* context, uint16_t overall_index);


// --- Global/Context Structure Initialization ---

/**
 * @brief Initializes or re-initializes the trade context.
 * @param context Pointer to the TradeContext to initialize.
 * @param is_master True if this device is the master, false if slave.
 * @param local_pokemon_storage_idx Index of the Pokemon in local storage to be traded.
 *                                  If -1, no Pokemon is initially selected for sending (can be set later).
 */
void trade_init(TradeContext* context, bool is_master, int8_t local_pokemon_storage_idx) {
    if (!context) return;

    memset(context, 0, sizeof(TradeContext)); // Clear the whole context

    context->is_master_role = is_master;
    context->timeout_ms = 5000; // Default 5 second timeout, can be adjusted
    context->last_comm_time_ms = 0; // Needs to be set by caller using get_current_time_ms()

    if (local_pokemon_storage_idx >= 0) {
        context->player_pokemon_index = (uint8_t)local_pokemon_storage_idx;
        trade_load_pokemon_to_send_buffer(context, context->player_pokemon_index);
        context->local_pokemon_selected = true;
    } else {
        context->player_pokemon_index = 0xFF; // Indicate no Pokemon selected yet
        context->local_pokemon_selected = false;
    }
    
    trade_reset_context_for_new_session(context); // Sets initial states
}

/**
 * @brief Resets relevant parts of the context for a new trading session or after completion/cancellation.
 */
static void trade_reset_context_for_new_session(TradeContext* context) {
    context->current_state = context->is_master_role ? TRADE_STATE_INIT_MASTER : TRADE_STATE_INIT_SLAVE;
    context->current_block_index = 0;
    context->total_block_size = 0;
    context->random_data_exchange_count = 0;
    context->remote_selected_pokemon_slot = 0xFF; // Invalid slot
    // context->local_pokemon_selected should be managed by UI/selection logic
    context->remote_pokemon_selected = false;
    context->local_trade_confirmed = false;
    context->remote_trade_confirmed = false;

    // Clear buffers if they contain sensitive data from previous trades
    // memset(context->send_buffer, 0, sizeof(context->send_buffer)); // Done by prepare functions
    // memset(context->receive_buffer, 0, sizeof(context->receive_buffer));

    // If a specific Pokemon was pre-loaded, keep it, otherwise ensure pokemon_to_send is clear
    if (!context->local_pokemon_selected) {
        memset(&context->pokemon_to_send, 0, sizeof(PokemonTradeUnit));
    }
    memset(&context->pokemon_received, 0, sizeof(PokemonTradeUnit));

    printf("Trade context reset. Initial state: %d\n", context->current_state);
}

/**
 * @brief Loads the selected Pokemon's data into the pokemon_to_send buffer in the context.
 */
static void trade_load_pokemon_to_send_buffer(TradeContext* context, uint8_t local_pokemon_storage_idx) {
    if (!context) return;

    PokemonTradeUnit* pkm_from_storage = get_pokemon_from_storage(local_pokemon_storage_idx);
    if (pkm_from_storage && pkm_from_storage->is_slot_occupied) {
        memcpy(&context->pokemon_to_send, pkm_from_storage, sizeof(PokemonTradeUnit));
        context->local_pokemon_storage_ptr = pkm_from_storage; // Store pointer for potential final swap
        printf("Loaded Pokemon from storage index %d for sending.\n", local_pokemon_storage_idx);
    } else {
        printf("Failed to load Pokemon from storage index %d.\n", local_pokemon_storage_idx);
        memset(&context->pokemon_to_send, 0, sizeof(PokemonTradeUnit)); // Clear if not found
        context->local_pokemon_storage_ptr = NULL;
        context->local_pokemon_selected = false; // Mark as not selected if load failed
    }
}

// --- Main Processing Function ---

/**
 * @brief Processes an incoming byte from the link cable and updates the trade state machine.
 * @param context Pointer to the TradeContext.
 * @param received_byte The byte received from the link cable.
 * @param byte_to_send_next Pointer to a variable where the next byte to send will be stored.
 */
void trade_process_byte(TradeContext* context, uint8_t received_byte, uint8_t* byte_to_send_next) {
    if (!context || !byte_to_send_next) return;

    // Update last communication time (caller should provide actual time source)
    // context->last_comm_time_ms = get_current_time_ms(); 
    context->last_byte_received = received_byte;
    *byte_to_send_next = PKMN_BLANK; // Default response, usually overridden

    printf("State: %d, Rcv: 0x%02X\n", context->current_state, received_byte);

    switch (context->current_state) {
        // --- Connection & Role Negotiation ---
        case TRADE_STATE_IDLE:
            // This state should ideally be handled by a higher-level function that calls trade_init
            // For now, assume trade_init was called and we are in INIT_MASTER or INIT_SLAVE
            if (context->is_master_role) {
                context->current_state = TRADE_STATE_INIT_MASTER;
                *byte_to_send_next = PKMN_MASTER;
            } else {
                context->current_state = TRADE_STATE_INIT_SLAVE;
                // Slave typically waits for PKMN_MASTER, but might send PKMN_SLAVE proactively
                *byte_to_send_next = PKMN_SLAVE; 
            }
            break;

        case TRADE_STATE_INIT_MASTER:
            if (received_byte == PKMN_SLAVE) {
                *byte_to_send_next = PKMN_CONNECTED; // Master sends CONNECTED
                context->current_state = TRADE_STATE_CONNECTION_ESTABLISHED;
                printf("Master: Slave detected. Connection established.\n");
            } else {
                *byte_to_send_next = PKMN_MASTER; // Keep sending master signal
            }
            break;

        case TRADE_STATE_INIT_SLAVE:
            if (received_byte == PKMN_MASTER) {
                *byte_to_send_next = PKMN_SLAVE; // Slave responds with SLAVE
            } else if (received_byte == PKMN_CONNECTED) { // Master acknowledged slave
                *byte_to_send_next = PKMN_CONNECTED; // Slave also sends CONNECTED
                context->current_state = TRADE_STATE_CONNECTION_ESTABLISHED;
                printf("Slave: Master acknowledged. Connection established.\n");
            } else {
                 // Waiting for PKMN_MASTER or PKMN_CONNECTED after sending SLAVE
                *byte_to_send_next = PKMN_SLAVE; // Or PKMN_BLANK if already sent SLAVE
            }
            break;

        case TRADE_STATE_CONNECTION_ESTABLISHED:
            if (received_byte == PKMN_TRADE_CENTRE) { // Partner wants to trade
                *byte_to_send_next = PKMN_TRADE_CENTRE; // Acknowledge
                context->current_state = TRADE_STATE_TC_INIT;
                context->current_block_index = 0;
                printf("Entering Trade Center.\n");
            } else if (received_byte == PKMN_COLOSSEUM) {
                // Handle Colosseum (out of scope)
                *byte_to_send_next = PKMN_COLOSSEUM;
            } else if (received_byte == PKMN_BREAK_LINK) {
                trade_reset_context_for_new_session(context);
                *byte_to_send_next = PKMN_BREAK_LINK;
            }
             else { // Keep exchanging CONNECTED until a mode is chosen
                *byte_to_send_next = PKMN_CONNECTED;
            }
            break;

        // --- Trade Center Specific States ---
        case TRADE_STATE_TC_INIT: // Master sends 0x00, Slave expects 0x00 then sends 0x00
            if (context->is_master_role) {
                *byte_to_send_next = PKMN_BLANK; // Master sends 0x00
                if (received_byte == PKMN_BLANK) { // Slave also sent 0x00
                    context->current_state = TRADE_STATE_TC_READY_TO_GO;
                }
            } else { // Slave
                if (received_byte == PKMN_BLANK) { // Master sent 0x00
                    *byte_to_send_next = PKMN_BLANK; // Slave sends 0x00
                    context->current_state = TRADE_STATE_TC_READY_TO_GO;
                } else {
                    // Waiting for 0x00 from master
                    *byte_to_send_next = PKMN_BLANK; // Or some other waiting byte
                }
            }
            if(context->current_state == TRADE_STATE_TC_READY_TO_GO) printf("TC_INIT complete.\n");
            break;

        case TRADE_STATE_TC_READY_TO_GO: // Both sides send/expect 0xFD
            *byte_to_send_next = TRADE_CENTRE_WAIT;
            if (received_byte == TRADE_CENTRE_WAIT) {
                context->current_state = TRADE_STATE_TC_SEEN_FIRST_WAIT;
                context->random_data_exchange_count = 0;
                 printf("TC_READY_TO_GO: Synced on 0xFD.\n");
            }
            break;

        case TRADE_STATE_TC_SEEN_FIRST_WAIT: // Slave sends first random byte, master echoes then sends its own random.
                                             // Master sends first random byte, slave echoes then sends its own.
            // This state is tricky. The Arduino code implies slave sends first non-0xFD byte.
            // Master echoes it, then they proceed to exchange a few more random bytes.
            if (context->is_master_role) {
                // Master receives slave's first random byte.
                // For this example, master will echo it. A real game sends its own distinct random data.
                *byte_to_send_next = received_byte; 
                context->random_data_exchange_count = 1; // Count this exchange
                context->current_state = TRADE_STATE_TC_EXCHANGING_RANDOM_DATA;
            } else { // Slave
                // Slave sent its first random byte (not 0xFD). Expecting master to echo or send its own.
                // For this example, slave sends its random byte.
                *byte_to_send_next = 0xA1; // Example slave random byte
                if(received_byte != TRADE_CENTRE_WAIT) { // Master sent its random byte
                    context->random_data_exchange_count = 1;
                    context->current_state = TRADE_STATE_TC_EXCHANGING_RANDOM_DATA;
                }
            }
            if(context->current_state == TRADE_STATE_TC_EXCHANGING_RANDOM_DATA) printf("TC_SEEN_FIRST_WAIT: First random byte exchanged.\n");
            break;

        case TRADE_STATE_TC_EXCHANGING_RANDOM_DATA:
            // Exchange a few more "random" bytes (e.g., 2 more after the first one)
            // For simplicity, we'll just echo back. Real game sends its own.
            *byte_to_send_next = received_byte + context->random_data_exchange_count; // Example: send something based on received
            context->random_data_exchange_count++;
            if (context->random_data_exchange_count >= RANDOM_DATA_BLOCK_SIZE_EXPECTED) { // e.g. 3 bytes total
                context->current_state = TRADE_STATE_TC_WAITING_FOR_MAIN_DATA;
                *byte_to_send_next = TRADE_CENTRE_WAIT; // Signal end of random data with 0xFD
                printf("TC_EXCHANGING_RANDOM_DATA: Random data exchange complete. Sent 0xFD.\n");
            }
            break;
        
        case TRADE_STATE_TC_WAITING_FOR_MAIN_DATA:
            if (received_byte == TRADE_CENTRE_WAIT) { // Both sides confirmed end of random data
                *byte_to_send_next = TRADE_CENTRE_WAIT; // Acknowledge
                // Now, the one who is "slave" in this part of data transfer sends first actual data byte
                // The one who is "master" sends 0xFD and waits for non-0xFD data.
                // Let's assume gameboy.ino's logic: Master sends 0xFD, slave sends first data byte.
                if (context->is_master_role) {
                    // Master has sent 0xFD, waiting for slave's first data byte.
                } else { // Slave
                    // Slave has sent 0xFD, now sends its first data byte.
                    trade_prepare_player_data_block(context); // Ensure send_buffer is ready
                    *byte_to_send_next = context->send_buffer[0];
                    context->current_block_index = 0; // Ready to send byte 0
                    context->receive_buffer[context->current_block_index] = received_byte; // This is master's 0xFD, ignore for data
                    // No, slave receives master's 0xFD, then sends data[0]. Next turn, master sends data[0].
                }
                 context->current_block_index = 0;
                 context->total_block_size = PLAYER_DATA_BLOCK_SIZE;
                 trade_prepare_player_data_block(context); // Prepare our data
                 context->current_state = TRADE_STATE_TC_EXCHANGING_MAIN_DATA;
                 printf("TC_WAITING_FOR_MAIN_DATA: Synced for main data. State -> EXCHANGING_MAIN_DATA\n");
                 // The first actual data byte exchange happens in the next state iteration.
                 // Master will send its data[0] after receiving slave's data[0].
                 // Slave will send its data[0] after receiving master's 0xFD.
                 // To simplify, let's assume the next byte from master will be data[0] if it received data[0]
                 // and next byte from slave will be data[0] if it received 0xFD.
                 // The gameboy.ino logic is clearer: master receives data[0], THEN sends its data[0].
                 if(context->is_master_role){
                    // Master already sent 0xFD, waiting for slave's first data byte.
                    // *byte_to_send_next will be set in next block if non-0xFD received.
                 } else { // Slave
                    *byte_to_send_next = context->send_buffer[0];
                 }

            } else { // Slave sent its first data byte, master now responds with its first.
                 if (context->is_master_role) {
                    context->receive_buffer[0] = received_byte; // Store slave's first byte
                    trade_prepare_player_data_block(context);    // Prepare our data block
                    *byte_to_send_next = context->send_buffer[0]; // Send our first byte
                    context->current_block_index = 1;             // Advance index
                    context->total_block_size = PLAYER_DATA_BLOCK_SIZE;
                    context->current_state = TRADE_STATE_TC_EXCHANGING_MAIN_DATA;
                    printf("TC_WAITING_FOR_MAIN_DATA: Master received data[0], sent data[0]. State -> EXCHANGING_MAIN_DATA\n");
                 } else {
                    // Slave should not be in this part of the 'else' if it's sending data[0]
                    // This indicates a logic error or unexpected byte.
                    *byte_to_send_next = PKMN_BLANK; // Error or resync
                 }
            }
            break;

        case TRADE_STATE_TC_EXCHANGING_MAIN_DATA:
            context->receive_buffer[context->current_block_index] = received_byte;
            *byte_to_send_next = context->send_buffer[context->current_block_index];
            context->current_block_index++;

            if (context->current_block_index >= context->total_block_size) { // PLAYER_DATA_BLOCK_SIZE
                trade_process_received_player_data(context); // Process the received Pokemon list
                context->current_state = TRADE_STATE_TC_EXCHANGING_PATCH_DATA;
                context->current_block_index = 0;
                context->total_block_size = PATCH_DATA_BLOCK_SIZE;
                trade_prepare_patch_data_block(context); // Prepare our patch data
                *byte_to_send_next = TRADE_CENTRE_WAIT; // Signal end of main data, start of patch data handshake
                printf("Main data exchange complete. Sent 0xFD for patch data.\n");
            }
            break;

        case TRADE_STATE_TC_EXCHANGING_PATCH_DATA:
            if (context->current_block_index == 0 && received_byte == TRADE_CENTRE_WAIT) {
                // Both sides acknowledged end of main data, ready for patch data.
                // Slave sends first patch byte, master receives then sends its first.
                *byte_to_send_next = TRADE_CENTRE_WAIT; // Master also sends 0xFD
                // The actual data exchange starts on the next byte.
            } else { // Exchanging patch data bytes
                 if (context->current_block_index == 0 && context->is_master_role && received_byte != TRADE_CENTRE_WAIT) {
                    // Master received slave's first patch byte
                    context->receive_buffer[context->current_block_index] = received_byte;
                    *byte_to_send_next = context->send_buffer[context->current_block_index];
                    context->current_block_index++;
                 } else if (context->current_block_index == 0 && !context->is_master_role && received_byte == TRADE_CENTRE_WAIT) {
                    // Slave received master's 0xFD, now sends its first patch byte
                     *byte_to_send_next = context->send_buffer[context->current_block_index];
                     // current_block_index remains 0, master will send its byte 0 in next turn.
                 }
                 else { // Subsequent bytes
                    context->receive_buffer[context->current_block_index] = received_byte;
                    *byte_to_send_next = context->send_buffer[context->current_block_index];
                    context->current_block_index++;
                 }

                if (context->current_block_index >= context->total_block_size) { // PATCH_DATA_BLOCK_SIZE
                    trade_process_received_patch_data(context);
                    context->current_state = TRADE_STATE_TC_AWAITING_SELECTION;
                    // What to send now? Usually a neutral byte or wait for partner's selection.
                    *byte_to_send_next = PKMN_BLANK; // Or specific "selection_pending" byte
                    printf("Patch data exchange complete. State -> AWAITING_SELECTION.\n");
                }
            }
            break;

        case TRADE_STATE_TC_AWAITING_SELECTION:
            // Player selects which Pokemon to trade (0-5 -> 0x60-0x65) or cancels (0x6F)
            // Or if both have selected, master sends 0x00 to proceed to final confirm screen.
            if (received_byte >= TRADE_ACTION_CONFIRM_SELECTION_PREFIX && received_byte < (TRADE_ACTION_CONFIRM_SELECTION_PREFIX + PARTY_SIZE)) {
                context->remote_selected_pokemon_slot = received_byte - TRADE_ACTION_CONFIRM_SELECTION_PREFIX;
                context->remote_pokemon_selected = true;
                printf("Remote selected slot %d.\n", context->remote_selected_pokemon_slot);
                if (context->local_pokemon_selected) { // If we also have selected one
                     // Master usually initiates move to next phase
                    if(context->is_master_role) *byte_to_send_next = PKMN_BLANK; // Master sends 0x00 to go to confirmation
                    else *byte_to_send_next = TRADE_ACTION_CONFIRM_SELECTION_PREFIX + context->player_pokemon_index; // Slave re-sends its choice
                } else {
                    // We haven't selected, UI should prompt us. Send back our current choice (which is nothing or old).
                    *byte_to_send_next = PKMN_BLANK; // Or a "waiting for local selection" byte
                }
            } else if (received_byte == TRADE_ACTION_CANCEL_SELECTION) { // 0x6F
                printf("Remote cancelled selection.\n");
                *byte_to_send_next = TRADE_ACTION_CANCEL_SELECTION; // Acknowledge
                trade_reset_context_for_new_session(context); // Or a softer reset to TC_INIT/READY_TO_GO
                // UI should reflect cancellation.
            } else if (received_byte == PKMN_BLANK && context->local_pokemon_selected && context->remote_pokemon_selected) {
                // This is likely Master signaling to move to confirmation screen
                *byte_to_send_next = PKMN_BLANK; // Acknowledge
                context->current_state = TRADE_STATE_TC_AWAITING_CONFIRMATION;
                printf("Both selected, moving to final confirmation.\n");
            } else {
                // Waiting for local player to select a Pokemon or remote to act
                // If local player just selected via UI:
                // if (ui_local_selected_pokemon) {
                //    context->local_pokemon_selected = true;
                //    *byte_to_send_next = TRADE_ACTION_CONFIRM_SELECTION_PREFIX + context->player_pokemon_index;
                // } else {
                //    *byte_to_send_next = PKMN_BLANK; // Default "waiting" byte
                // }
                *byte_to_send_next = PKMN_BLANK; // Placeholder
            }
            break;

        case TRADE_STATE_TC_AWAITING_CONFIRMATION: // Final "TRADE?" Yes (0x60) / No (0x61)
            if (received_byte == TRADE_ACTION_CONFIRM_SELECTION_PREFIX) { // Remote said YES (0x60)
                context->remote_trade_confirmed = true;
                printf("Remote confirmed trade (YES).\n");
                if (context->local_trade_confirmed) { // If we also said YES
                    *byte_to_send_next = TRADE_ACTION_CONFIRM_SELECTION_PREFIX; // Confirm our YES
                    context->current_state = TRADE_STATE_TC_TRADE_CONFIRMED;
                    // Perform the actual Pokemon data swap in memory/storage here!
                    // e.g. add_pokemon_to_storage(&context->pokemon_received);
                    //      pokemon_storage_array[context->player_pokemon_index].is_slot_occupied = 0; // "Remove" traded one
                    printf("TRADE CONFIRMED by both! Pokemon should be swapped.\n");
                } else {
                    // We are waiting for local confirmation. Send back their 0x60 for now.
                    *byte_to_send_next = TRADE_ACTION_CONFIRM_SELECTION_PREFIX;
                }
            } else if (received_byte == TRADE_ACTION_CANCEL_FINAL) { // Remote said NO (0x61)
                printf("Remote cancelled final confirmation (NO).\n");
                *byte_to_send_next = TRADE_ACTION_CANCEL_FINAL; // Acknowledge
                context->current_state = TRADE_STATE_TC_AWAITING_SELECTION; // Back to selection screen
                context->remote_trade_confirmed = false;
                context->local_trade_confirmed = false; // Reset local if it was set
                context->remote_pokemon_selected = false; // Reset this too
            } else {
                // Waiting for remote confirmation, or local player to confirm via UI
                // if (ui_local_final_confirmation_yes) {
                //    context->local_trade_confirmed = true;
                //    *byte_to_send_next = TRADE_ACTION_CONFIRM_SELECTION_PREFIX; // Send 0x60
                //    if(context->remote_trade_confirmed) context->current_state = TRADE_STATE_TC_TRADE_CONFIRMED;
                // } else if (ui_local_final_confirmation_no) {
                //    *byte_to_send_next = TRADE_ACTION_CANCEL_FINAL; // Send 0x61
                //    context->current_state = TRADE_STATE_TC_AWAITING_SELECTION;
                // } else {
                //    *byte_to_send_next = PKMN_BLANK; // Waiting
                // }
                *byte_to_send_next = PKMN_BLANK; // Placeholder
            }
            break;
        
        case TRADE_STATE_TC_TRADE_CONFIRMED:
            // Trade is logically done. Game Boy will send a final 0x00.
            // The actual saving of the traded Pokemon (from context->pokemon_received) should happen here or be triggered.
            // The removal/marking of the sent Pokemon (context->pokemon_to_send) from local storage also.
            // For example, if using pokemon_storage.c:
            // if (context->local_pokemon_storage_ptr) {
            //    context->local_pokemon_storage_ptr->is_slot_occupied = 0; // Mark as traded away
            // }
            // add_pokemon_to_storage(&context->pokemon_received);
            *byte_to_send_next = PKMN_BLANK; // Send 0x00 to acknowledge completion
            if (received_byte == PKMN_BLANK) {
                context->current_state = TRADE_STATE_TC_COMPLETE_WAIT_ACK; // Or directly to COMPLETE
                printf("Trade confirmed, sent 0x00, waiting for final 0x00 from partner.\n");
            }
            break;

        case TRADE_STATE_TC_COMPLETE_WAIT_ACK: // Both sides sent 0x00, trade is done.
            *byte_to_send_next = PKMN_BLANK; // Keep sending 0x00
            if (received_byte == PKMN_BLANK) {
                 context->current_state = TRADE_STATE_TC_COMPLETE;
                 printf("Trade fully complete. Ready for new session or disconnect.\n");
            }
            break;
            
        case TRADE_STATE_TC_COMPLETE:
            // Stay in this state, send PKMN_BLANK until external action (e.g. new trade, or disconnect)
            // This state indicates the game is showing "Trade Completed!"
            // Returning to Cable Club menu might reset state to TRADE_STATE_CONNECTION_ESTABLISHED
            *byte_to_send_next = PKMN_BLANK;
            if (received_byte == PKMN_BREAK_LINK || received_byte == PKMN_MASTER) { // Partner disconnected or reset
                trade_reset_context_for_new_session(context);
                *byte_to_send_next = PKMN_BREAK_LINK;
            } else if (received_byte == PKMN_TRADE_CENTRE) { // Partner wants another trade
                trade_reset_context_for_new_session(context); // Soft reset for new trade
                context->current_state = TRADE_STATE_TC_INIT;
                 *byte_to_send_next = PKMN_TRADE_CENTRE;
            }
            break;

        case TRADE_STATE_ERROR:
            // Attempt to reset or send error signal
            *byte_to_send_next = PKMN_BREAK_LINK; // Or a specific error byte
            trade_reset_context_for_new_session(context);
            break;

        default:
            // Should not happen
            context->current_state = TRADE_STATE_ERROR;
            *byte_to_send_next = PKMN_BREAK_LINK;
            break;
    }
    context->last_byte_sent = *byte_to_send_next;
    // printf("Sent: 0x%02X\n", *byte_to_send_next);
}


/**
 * @brief Prepares the PLAYER_DATA_BLOCK_SIZE buffer with data to be sent.
 * This involves copying the selected Pokemon's data into the correct part of a template.
 * The template itself (representing the other party members, player name etc.) is simplified here.
 * In a real game, this block is complex. This function focuses on placing the traded Pokemon's data.
 */
void trade_prepare_player_data_block(TradeContext* context) {
    if (!context) return;

    // Create a template DATA_BLOCK (like in gameboy.ino)
    // For this example, we'll mostly zero it and fill in the specific Pokemon.
    // A real implementation would get this template from the game's current state.
    memset(context->send_buffer, 0x50, PLAYER_DATA_BLOCK_SIZE); // Fill with a placeholder like 0x50

    if (context->local_pokemon_selected && context->pokemon_to_send.is_slot_occupied) {
        // Species ID (special handling in gameboy.ino at index 12 of DATA_BLOCK)
        // This needs to align with how the Game Boy expects the full block.
        // For now, assume the full PokemonTradeUnit is being sent as part of a larger structure.
        // The gameboy.ino example injects parts of a single Pokemon into a shared party block.

        // Example: Placing the 66-byte PokemonTradeUnit at a conceptual offset
        // This is a simplification. The real game builds this block from wPlayerTradableMons.
        // For the Arduino project, it constructs DATA_BLOCK from various sources.
        // Let's simulate injecting the selected Pokemon's data into the send_buffer
        // based on offsets seen in gameboy.ino's DATA_BLOCK construction.

        // 1. Species (at effective DATA_BLOCK index 12 in gameboy.ino's example)
        //    For simplicity, we'll assume our PokemonMainData struct starts with species.
        //    The actual game might have a list of species first, then full data blocks.
        //    If DATA_BLOCK[12] is species of first Pokemon:
        if (PLAYER_DATA_BLOCK_SIZE > 12) { // Basic sanity check
           context->send_buffer[12] = context->pokemon_to_send.main_data.species_id;
        }

        // 2. Main Pokemon Data (44 bytes)
        //    (gameboy.ino DATA_BLOCK indices 19 to 19+44-1 for the *first* Pokemon in party)
        //    This implies a structure for 6 Pokemon. We're sending data for one.
        //    We'll place our selected Pokemon's data as if it's the first in the list.
        uint16_t main_data_offset = 19; // Starting offset in the 512-byte block for first Pokemon's data
        if (PLAYER_DATA_BLOCK_SIZE > main_data_offset + POKEMON_MAIN_DATA_SIZE) {
            memcpy(&context->send_buffer[main_data_offset], &context->pokemon_to_send.main_data, POKEMON_MAIN_DATA_SIZE);
        }

        // 3. OT Name (11 bytes)
        //    (gameboy.ino DATA_BLOCK indices 283 to 283+11-1 for the *first* Pokemon's OT)
        uint16_t ot_name_offset = 283;
        if (PLAYER_DATA_BLOCK_SIZE > ot_name_offset + OT_NAME_BUFFER_SIZE) {
            memcpy(&context->send_buffer[ot_name_offset], &context->pokemon_to_send.ot_name, OT_NAME_BUFFER_SIZE);
        }

        // 4. Nickname (11 bytes)
        //    (gameboy.ino DATA_BLOCK indices 349 to 349+11-1 for the *first* Pokemon's Nickname)
        uint16_t nickname_offset = 349;
        if (PLAYER_DATA_BLOCK_SIZE > nickname_offset + NICKNAME_BUFFER_SIZE) {
            memcpy(&context->send_buffer[nickname_offset], &context->pokemon_to_send.nickname, NICKNAME_BUFFER_SIZE);
        }
        printf("Prepared player data block for sending with Pokemon from slot %d.\n", context->player_pokemon_index);
    } else {
        // No specific Pokemon selected or loaded, send a default/empty block
        // (memset to 0x50 already did this, or send actual blanks 0x00)
        printf("No local Pokemon loaded for trade, sending default block.\n");
    }
}

/**
 * @brief Processes the received PLAYER_DATA_BLOCK_SIZE into context->pokemon_received.
 * This simplified version assumes the *first* Pokemon in the received block is the one being traded.
 * A real implementation would need to know which slot the partner chose.
 */
void trade_process_received_player_data(TradeContext* context) {
    if (!context) return;

    // Assume the remote player's selection (remote_selected_pokemon_slot) indicates
    // which Pokemon's data to extract from the received block.
    // The received block (context->receive_buffer) is structured like the send_buffer.
    // We need to extract the species, main_data, ot_name, and nickname for the *selected* remote Pokemon.

    // For now, let's assume the offsets are for the *first* Pokemon in the block,
    // and that this corresponds to the one being traded.
    // This needs to be more robust based on remote_selected_pokemon_slot.

    uint16_t species_offset = 12; // Per gameboy.ino DATA_BLOCK
    if (PLAYER_DATA_BLOCK_SIZE > species_offset) {
        context->pokemon_received.main_data.species_id = context->receive_buffer[species_offset];
    }

    uint16_t main_data_offset = 19;
    if (PLAYER_DATA_BLOCK_SIZE > main_data_offset + POKEMON_MAIN_DATA_SIZE) {
        memcpy(&context->pokemon_received.main_data, &context->receive_buffer[main_data_offset], POKEMON_MAIN_DATA_SIZE);
    }

    uint16_t ot_name_offset = 283;
    if (PLAYER_DATA_BLOCK_SIZE > ot_name_offset + OT_NAME_BUFFER_SIZE) {
        memcpy(&context->pokemon_received.ot_name, &context->receive_buffer[ot_name_offset], OT_NAME_BUFFER_SIZE);
    }

    uint16_t nickname_offset = 349;
    if (PLAYER_DATA_BLOCK_SIZE > nickname_offset + NICKNAME_BUFFER_SIZE) {
        memcpy(&context->pokemon_received.nickname, &context->receive_buffer[nickname_offset], NICKNAME_BUFFER_SIZE);
    }
    context->pokemon_received.is_slot_occupied = 1; // Mark as valid data received
    printf("Processed received player data block. Received Pokemon species: %d\n", context->pokemon_received.main_data.species_id);
}

/**
 * @brief Prepares the PATCH_DATA_BLOCK_SIZE buffer.
 * In the Arduino code, this was just echoing received bytes.
 * A real game sends its own patch data. For now, fill with a placeholder.
 */
void trade_prepare_patch_data_block(TradeContext* context) {
    if (!context) return;
    // Fill with a placeholder pattern, e.g., 0xAB
    memset(context->send_buffer, 0xAB, PATCH_DATA_BLOCK_SIZE);
    printf("Prepared patch data block for sending.\n");
}

/**
 * @brief Processes the received PATCH_DATA_BLOCK_SIZE.
 * Currently, this data's purpose is not fully clear from gameboy.ino, so we just "receive" it.
 */
void trade_process_received_patch_data(TradeContext* context) {
    if (!context) return;
    // The received data is in context->receive_buffer (up to PATCH_DATA_BLOCK_SIZE)
    // No specific processing defined for this data yet.
    printf("Processed (ignored) received patch data block.\n");
}


/**
 * @brief Placeholder for checking for link cable timeouts.
 * @param context Pointer to the TradeContext.
 */
void check_trade_timeout(TradeContext* context) {
    if (!context) return;
    // Example:
    // if (get_current_time_ms() - context->last_comm_time_ms > context->timeout_ms) {
    //     printf("Trade timed out. State: %d\n", context->current_state);
    //     context->current_state = TRADE_STATE_ERROR;
    // }
}

/**
 * @brief Selects a Pokemon from local storage to be traded.
 * @param context Pointer to the TradeContext.
 * @param storage_idx Index of the Pokemon in local storage.
 * @return True if selection was successful, false otherwise.
 */
bool trade_select_local_pokemon(TradeContext* context, uint8_t storage_idx) {
    if (!context || storage_idx >= MAX_POKEMON_STORAGE) return false;

    PokemonTradeUnit* pkm = get_pokemon_from_storage(storage_idx);
    if (pkm && pkm->is_slot_occupied) {
        context->player_pokemon_index = storage_idx;
        trade_load_pokemon_to_send_buffer(context, storage_idx); // Load its data into pokemon_to_send
        context->local_pokemon_selected = true;
        printf("Local Pokemon for trade selected: Slot %d, Species %d\n", storage_idx, context->pokemon_to_send.main_data.species_id);
        return true;
    }
    printf("Failed to select local Pokemon: Slot %d empty or invalid.\n", storage_idx);
    return false;
}

/**
 * @brief Sets the local player's final confirmation for the trade.
 * This would be called by the UI when the player hits "YES" or "NO".
 * @param context Pointer to the TradeContext.
 * @param confirmed True if "YES", false if "NO".
 */
void trade_set_local_confirmation(TradeContext* context, bool confirmed) {
    if (!context) return;
    if (context->current_state == TRADE_STATE_TC_AWAITING_CONFIRMATION) {
        context->local_trade_confirmed = confirmed;
        printf("Local trade confirmation set to: %s\n", confirmed ? "YES" : "NO");
        // The trade_process_byte function will then send the appropriate 0x60 or 0x61
        // when it next runs, based on this flag and the partner's byte.
    }
}

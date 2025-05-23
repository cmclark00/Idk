#ifndef POKEMON_STORAGE_H
#define POKEMON_STORAGE_H

#include <stdint.h> // For fixed-width integer types like uint8_t, uint16_t
#include <stddef.h> // For NULL

// --- Constants ---
#define POKEMON_MAIN_DATA_SIZE 44
#define OT_NAME_MAX_LEN 10 // Max characters for OT name
#define NICKNAME_MAX_LEN 10 // Max characters for nickname
#define OT_NAME_BUFFER_SIZE (OT_NAME_MAX_LEN + 1) // Includes null terminator
#define NICKNAME_BUFFER_SIZE (NICKNAME_MAX_LEN + 1) // Includes null terminator

#define MAX_POKEMON_STORAGE 20 // Maximum number of Pokemon that can be stored

// --- Individual Data Blocks ---

// Main 44-byte Pokemon data block
typedef struct {
    uint8_t species_id;
    uint16_t current_hp;
    uint8_t level_box; // "Box level" or level again
    uint8_t status_condition;
    uint8_t type1;
    uint8_t type2;
    uint8_t catch_rate_or_held_item;
    uint8_t move1_id;
    uint8_t move2_id;
    uint8_t move3_id;
    uint8_t move4_id;
    uint16_t original_trainer_id;
    uint8_t experience[3];
    uint16_t hp_ev;
    uint16_t attack_ev;
    uint16_t defense_ev;
    uint16_t speed_ev;
    uint16_t special_ev;
    uint16_t iv_data;
    uint8_t move1_pp;
    uint8_t move2_pp;
    uint8_t move3_pp;
    uint8_t move4_pp;
    uint8_t level; // Actual level
    uint16_t max_hp;
    uint16_t attack;
    uint16_t defense;
    uint16_t speed;
    uint16_t special;
    // Total = 44 bytes
} PokemonMainData;

// OT Name
typedef struct {
    char name[OT_NAME_BUFFER_SIZE];
} PokemonOTName;

// Nickname
typedef struct {
    char name[NICKNAME_BUFFER_SIZE];
} PokemonNickname;

// --- Combined Tradeable Unit ---
// Represents a single Pokemon prepared for trade or storage.
typedef struct {
    PokemonMainData main_data;   // 44 bytes
    PokemonOTName ot_name;       // 11 bytes
    PokemonNickname nickname;    // 11 bytes
    uint8_t is_slot_occupied;    // Flag to indicate if this storage slot is used
} PokemonTradeUnit; // Total 66 bytes + 1 flag byte


// --- Management Functions Declarations ---

int initialize_pokemon_storage();
int add_pokemon_to_storage(const PokemonTradeUnit* pokemon_to_add); // Takes a const pointer
PokemonTradeUnit* get_pokemon_from_storage(int index);
int get_stored_pokemon_count();
void list_stored_pokemon_names(); // For debugging

#endif // POKEMON_STORAGE_H

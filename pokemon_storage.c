#include "pokemon_storage.h"
#include <string.h> // For memcpy and memset
#include <stdio.h>  // For printf in list_stored_pokemon_names

// --- Global Storage ---
static PokemonTradeUnit pokemon_storage_array[MAX_POKEMON_STORAGE];
static int current_pokemon_count = 0;

// --- Management Functions Implementations ---

/**
 * @brief Initializes the Pokemon storage.
 * Clears all storage slots and resets the count of stored Pokemon.
 *
 * @return int 0 on success.
 */
int initialize_pokemon_storage() {
    for (int i = 0; i < MAX_POKEMON_STORAGE; ++i) {
        memset(&pokemon_storage_array[i], 0, sizeof(PokemonTradeUnit));
        pokemon_storage_array[i].is_slot_occupied = 0; // Mark as not occupied
    }
    current_pokemon_count = 0;
    return 0; // Success
}

/**
 * @brief Adds a copy of the given Pokemon to the next available slot in storage.
 *
 * @param pokemon_to_add Pointer to the PokemonTradeUnit to add. The data will be copied.
 * @return int The index where the Pokemon was stored, or -1 if storage is full or pokemon_to_add is NULL.
 */
int add_pokemon_to_storage(const PokemonTradeUnit* pokemon_to_add) {
    if (pokemon_to_add == NULL) {
        return -1; // Null pointer passed
    }

    if (current_pokemon_count >= MAX_POKEMON_STORAGE) {
        return -1; // Storage is full
    }

    // Find the next truly empty slot (should generally be current_pokemon_count if no deletes)
    // For simplicity, we add at current_pokemon_count and increment.
    // A more robust implementation might search for the first slot where is_slot_occupied is 0.
    int storage_index = -1;
    for(int i = 0; i < MAX_POKEMON_STORAGE; ++i) {
        if (!pokemon_storage_array[i].is_slot_occupied) {
            storage_index = i;
            break;
        }
    }

    if (storage_index == -1) { // Should not happen if current_pokemon_count < MAX_POKEMON_STORAGE
        return -1; // No free slot found (inconsistency)
    }

    memcpy(&pokemon_storage_array[storage_index], pokemon_to_add, sizeof(PokemonTradeUnit));
    pokemon_storage_array[storage_index].is_slot_occupied = 1; // Mark as occupied

    // Only increment count if we are filling a new logical slot,
    // not just an empty one from a previous deletion.
    // For this simple array, we just increment the overall count.
    // A more complex system would handle gaps.
    current_pokemon_count++; // This reflects the number of occupied slots.

    return storage_index;
}

/**
 * @brief Retrieves a pointer to the Pokemon at the given index in storage.
 *
 * @param index The index of the Pokemon to retrieve.
 * @return PokemonTradeUnit* Pointer to the PokemonTradeUnit at the given index,
 *         or NULL if the index is out of bounds or the slot is not occupied.
 */
PokemonTradeUnit* get_pokemon_from_storage(int index) {
    if (index < 0 || index >= MAX_POKEMON_STORAGE) {
        return NULL; // Index out of bounds
    }
    if (!pokemon_storage_array[index].is_slot_occupied) {
        return NULL; // Slot is empty
    }
    return &pokemon_storage_array[index];
}

/**
 * @brief Gets the current number of Pokemon stored.
 *
 * @return int The number of Pokemon currently in storage.
 */
int get_stored_pokemon_count() {
    // Recalculate by checking flags, to be more robust if add/remove gets complex
    int count = 0;
    for (int i = 0; i < MAX_POKEMON_STORAGE; ++i) {
        if (pokemon_storage_array[i].is_slot_occupied) {
            count++;
        }
    }
    current_pokemon_count = count; // Update the global count
    return current_pokemon_count;
}

/**
 * @brief Lists the nicknames (or species ID if nickname is empty) of all stored Pokemon.
 * (For debugging purposes)
 */
void list_stored_pokemon_names() {
    printf("--- Stored Pokemon (%d/%d) ---\n", get_stored_pokemon_count(), MAX_POKEMON_STORAGE);
    for (int i = 0; i < MAX_POKEMON_STORAGE; ++i) {
        if (pokemon_storage_array[i].is_slot_occupied) {
            // Check if nickname is effectively empty (first char is null terminator or all spaces)
            // For simplicity, we'll check for a null terminator at the start.
            if (pokemon_storage_array[i].nickname.name[0] == '\0') {
                printf("Slot %d: Species ID %u (No Nickname)\n", i, pokemon_storage_array[i].main_data.species_id);
            } else {
                printf("Slot %d: %s (Species ID %u)\n", i, pokemon_storage_array[i].nickname.name, pokemon_storage_array[i].main_data.species_id);
            }
        } else {
            // Optional: print empty slots if needed for verbose debugging
            // printf("Slot %d: Empty\n", i);
        }
    }
    printf("-----------------------------\n");
}

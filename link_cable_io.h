#ifndef LINK_CABLE_IO_H
#define LINK_CABLE_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h" // ESP-IDF GPIO driver

// --- GPIO Pin Definitions ---
// These can be changed based on hardware layout
#define LINK_CABLE_SOUT_PIN  GPIO_NUM_23 // ESP32 Data Out (to GB SIN)
#define LINK_CABLE_SIN_PIN   GPIO_NUM_22 // ESP32 Data In (from GB SOUT)
#define LINK_CABLE_SCK_PIN   GPIO_NUM_19 // Serial Clock (ESP32 generates if master)

// --- Timing Constants ---
// Game Boy clock speed is ~8192 Hz for serial transfers
// Period = 1 / 8192 Hz = ~122.07 microseconds
// Half period (for high/low state of clock) = ~61 microseconds
#define GB_SERIAL_CLOCK_HZ 8192
#define GB_SERIAL_CLOCK_PERIOD_US (1000000 / GB_SERIAL_CLOCK_HZ)
#define GB_SERIAL_CLOCK_HALF_PERIOD_US (GB_SERIAL_CLOCK_PERIOD_US / 2)


// --- Function Declarations ---

/**
 * @brief Initializes the GPIO pins for link cable communication.
 * 
 * @param is_master_mode True if the ESP32 should initialize as master (SCK as output),
 *                       false if slave (SCK as input).
 */
void link_cable_init(bool is_master_mode);

/**
 * @brief Sends and receives a single byte over the link cable.
 * 
 * This function handles the synchronous serial protocol for one byte.
 * Timing is critical.
 * 
 * @param byte_to_send The byte for the ESP32 to send to the Game Boy.
 * @param is_master_mode True if the ESP32 is currently acting as the master 
 *                       (generating clock pulses). False if slave (waiting for clock pulses).
 * @return uint8_t The byte received from the Game Boy.
 */
uint8_t link_cable_send_receive_byte(uint8_t byte_to_send, bool is_master_mode);

/**
 * @brief Checks if there's an incoming signal from the Game Boy (e.g., initial clock pulse in slave mode).
 * This function is a simplified way to check for activity.
 * For master mode, it's less relevant as master initiates.
 * For slave mode, it can check if SCK is being driven by the Game Boy.
 * 
 * @return bool True if an incoming signal/activity is detected, false otherwise.
 *              (Note: Implementation might be basic and require more sophisticated handling
 *               in a full application, e.g. using interrupts for slave clock detection).
 */
bool link_cable_has_incoming_byte_signal(void); // More relevant for slave mode initial detection

#endif // LINK_CABLE_IO_H

#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <stdint.h>

// Max command buffer size
#define SERIAL_CMD_BUFFER_SIZE 128

// --- Public Function Declarations ---

/**
 * @brief Initializes the serial command processor.
 * (Currently does nothing, but good practice to have an init function).
 */
void serial_protocol_init(void);

/**
 * @brief Processes a single character received from the serial input.
 * Buffers characters until a newline is detected, then parses and handles the command.
 * This function should be called by the main loop or a serial input task whenever a new character is available.
 * 
 * @param received_char The character received from the serial port.
 */
void serial_protocol_process_char(char received_char);

/**
 * @brief Checks for and processes a complete command from the serial buffer if available.
 * Alternative to process_char if input is handled by polling a buffer filled by DMA/interrupt.
 * This function would typically be called repeatedly in the main loop.
 */
// void serial_protocol_check_and_process_command(void); // Alternative design

#endif // SERIAL_PROTOCOL_H

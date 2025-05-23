#include "link_cable_io.h"
#include "rom/ets_sys.h" // For ets_delay_us()
#include "esp_log.h"

static const char *TAG = "link_cable_io";

// --- Initialization ---
void link_cable_init(bool is_master_mode) {
    gpio_config_t io_conf;

    // Configure SOUT (ESP32 Data Out)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LINK_CABLE_SOUT_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(LINK_CABLE_SOUT_PIN, 0); // Default to LOW

    // Configure SIN (ESP32 Data In)
    io_conf.intr_type = GPIO_INTR_DISABLE; // Interrupts could be used for SCK in slave mode
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << LINK_CABLE_SIN_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1; // Optional: Pull-up if the Game Boy line might float
    gpio_config(&io_conf);

    // Configure SCK (Serial Clock)
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << LINK_CABLE_SCK_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0; 
    if (is_master_mode) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        gpio_set_level(LINK_CABLE_SCK_PIN, 1); // Default clock HIGH for master
        ESP_LOGI(TAG, "Link cable initialized in MASTER mode.");
    } else {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1; // Slave should expect clock, pull-up if it might float
        ESP_LOGI(TAG, "Link cable initialized in SLAVE mode.");
    }
    gpio_config(&io_conf);
}

// --- Low-Level Byte Transfer ---
uint8_t link_cable_send_receive_byte(uint8_t byte_to_send, bool is_master_mode) {
    uint8_t received_byte = 0;
    // ESP_LOGD(TAG, "Sending: 0x%02X, Master: %d", byte_to_send, is_master_mode);

    for (int i = 0; i < 8; i++) { // MSB first
        bool bit_to_send = (byte_to_send & (0x80 >> i));

        if (is_master_mode) {
            // Master generates clock
            gpio_set_level(LINK_CABLE_SOUT_PIN, bit_to_send);
            
            // Clock LOW
            gpio_set_level(LINK_CABLE_SCK_PIN, 0);
            ets_delay_us(GB_SERIAL_CLOCK_HALF_PERIOD_US); // Delay for half period

            // Read SIN while clock is LOW (or on rising/falling edge, GB standard is often read after clock edge)
            // Game Boy typically shifts data out on falling edge of clock, latches data in on rising edge.
            // If ESP32 (master) sets data, then drops clock, GB slave sees data.
            // If ESP32 (master) then raises clock, GB slave latches data.
            // ESP32 (master) should read when data from slave is stable.
            // Reading in the middle of the LOW or HIGH phase of master's clock is common.
            // Let's read after clock goes LOW, before it goes HIGH again.
            if (gpio_get_level(LINK_CABLE_SIN_PIN)) {
                received_byte |= (0x80 >> i);
            }

            // Clock HIGH
            gpio_set_level(LINK_CABLE_SCK_PIN, 1);
            ets_delay_us(GB_SERIAL_CLOCK_HALF_PERIOD_US); // Delay for half period

        } else { // Slave mode
            // Slave waits for clock edges from Master (Game Boy)
            
            // Wait for SCK to go LOW (start of clock pulse)
            // Timeout can be added here if necessary
            uint32_t timeout_start = esp_log_timestamp(); // Using timestamp for a simple timeout
            while (gpio_get_level(LINK_CABLE_SCK_PIN) == 1) {
                if ((esp_log_timestamp() - timeout_start) > (GB_SERIAL_CLOCK_PERIOD_US * 2 / 1000)) { // Timeout e.g. 2 clock periods in ms
                    ESP_LOGW(TAG, "Slave: Timeout waiting for SCK LOW (bit %d)", i);
                    return 0xFF; // Error/timeout byte
                }
            }

            // SCK is LOW
            // Game Boy (Master) has likely set its SOUT line now. Read it.
            if (gpio_get_level(LINK_CABLE_SIN_PIN)) {
                received_byte |= (0x80 >> i);
            }
            // Set our SOUT line for the Game Boy to read when it decides to.
            gpio_set_level(LINK_CABLE_SOUT_PIN, bit_to_send);

            // Wait for SCK to go HIGH (end of clock pulse)
            timeout_start = esp_log_timestamp();
            while (gpio_get_level(LINK_CABLE_SCK_PIN) == 0) {
                 if ((esp_log_timestamp() - timeout_start) > (GB_SERIAL_CLOCK_PERIOD_US * 2 / 1000)) {
                    ESP_LOGW(TAG, "Slave: Timeout waiting for SCK HIGH (bit %d)", i);
                    return 0xFF; // Error/timeout byte
                }
            }
            // SCK is HIGH, bit transfer complete for this bit
        }
    }
    // ESP_LOGD(TAG, "Received: 0x%02X", received_byte);
    return received_byte;
}

// --- Incoming Signal Detection (Simplified) ---
bool link_cable_has_incoming_byte_signal(void) {
    // In slave mode, this could check if SCK is active (being driven by master).
    // For now, this is a simplified check. A robust implementation might use
    // an interrupt on SCK's falling edge for slave mode to "wake up" the processing.
    // If master, this isn't really applicable as master initiates.
    
    // A simple heuristic: if SCK is low, it might be the start of a master's pulse.
    // This is not very reliable.
    // if (gpio_get_level(LINK_CABLE_SCK_PIN) == 0) {
    //     return true;
    // }
    // For now, let the main trade protocol logic decide when to attempt a read/write.
    return false; 
}

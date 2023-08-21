#include "cg_rfid.h"
#include "Wire.h"
#include "esphome.h"
using namespace esphome;

// Public Methods /////////////////////////////////////////////////////////////

CG_RFID *CG_RFID::instance_ = nullptr;

void CG_RFID::setup() {
    instance_ = this;
    
    if(storage.load(&cycle_counter)) {
        ESP_LOGI(TAG, "number of cycles loaded from preferences");
    } else {
        ESP_LOGI(TAG, "can't load number of cycles, using default 0");
    }
    BLOCK_1[1] = cycle_counter;

    Wire.begin(I2C_ADDRESS, I2C0_SDA, I2C0_SCL, I2C_FREQUENCY);
    Wire.onRequest(handle_request_event);
    Wire.onReceive(handle_receive_event);

    Wire1.begin(I2C1_SDA, I2C1_SCL);
}

void CG_RFID::reset() {
    BLOCK_1[1] = 120;
}

void CG_RFID::dump_config() {
    ESP_LOGCONFIG(TAG, "Bypass switch: %d", bpswitch->state);
    ESP_LOGCONFIG(TAG, "Cycle counter: %d", cycle_counter);
}

void CG_RFID::update() {
    if (cycle_counter != BLOCK_1[1]) {
        cycle_counter=BLOCK_1[1];
        storage.save(&cycle_counter);
        global_preferences->sync();
        ESP_LOGI(TAG, "preferences saved, cycle counter: %d", cycle_counter);
    }
}

// Private Methods //////////////////////////////////////////////////////////////

void CG_RFID::receive_event(int length) {
    if (bpswitch->state) {
        Wire1.beginTransmission(I2C_ADDRESS);
        for (int i = 0 ; Wire.available() ; i++) {
            Wire1.write(Wire.read());
        }
        Wire1.endTransmission();
        return;
    }

    // skip single byte write as its setting up the next read
    // and reading the ack breaks the following read
    if (length == 1) {
        // consume data but ignore
        Wire.read();
        return;
    }

    // all other instructions set the read register for a subsiquent block read (handled by requestEvent)
    for (int i = 0 ; Wire.available() ; i++) {
        read_register[i] = Wire.read();
    }

    int command = read_register[2] & 0x0f; // RFID Tag command

    switch (command) {
        case 0x06: // INITIATE, PCALL16, SLOT_MARKER
            response = NODE_ID_RESPONSE;
            size = 2;
            break;
        case 0x08: // RESET_BLOCK
            response = BLOCKS[read_register[3]];
            size = 5;
            break;
        case 0x09: // WRITE_BLOCK
            response = BLOCKS[read_register[3]];
            response[1] = read_register[4];
            response[2] = read_register[5];
            response[3] = read_register[6];
            response[4] = read_register[7];
            size = 0;
            break;
        case 0x0A: // AUTHENTICATE (ignored)
            size = 0;
            break;
        case 0x0B: // GET_UID
            response = UID_RESPONSE;
            size = 9;
            break;
        case 0x0C: // RESET_TO_INVENTORY
            size = 0;
            break;
        case 0x0E: // SELECT
            response = NODE_ID_RESPONSE;
            size = 2;
            break;
        case 0x0F: // COMPLETION
            size = 0;
            break;
        default:
            break;
    }
}

void CG_RFID::request_event() {
    if (bpswitch->state) {
        Wire1.requestFrom(I2C_ADDRESS, size);
        if (Wire1.available() >= size) {
            for (int i = 0 ; Wire1.available() ; i++) {       
                Wire.write(Wire1.read());
            }
        }
        return;
    }

    Wire.write(response, size);
}

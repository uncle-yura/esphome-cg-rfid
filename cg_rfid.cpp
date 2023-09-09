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

    Wire.begin(I2C_ADDRESS, I2C0_SDA, I2C0_SCL, I2C0_FREQUENCY);
    Wire.onRequest(handle_request_event);
    Wire.onReceive(handle_receive_event);

    Wire1.begin(I2C1_SDA, I2C1_SCL, I2C1_FREQUENCY);
    Wire1.setTimeOut(10);
}

void CG_RFID::update() {
    if (cycle_counter != BLOCK_1[1]) {
        cycle_counter=BLOCK_1[1];
        storage.save(&cycle_counter);
        global_preferences->sync();
        ESP_LOGI(TAG, "preferences saved, cycle counter: %d", cycle_counter);
    }

    if (bpswitch->state) {
        unsigned long time = millis();
        switch(refresh_step) {
            case 1:
                rfid_reset();
                read_node_id();
                select_node();
                break;
            case 2:
                read_uid();
                break;
            case 3:
                read_blocks(0, 4);
                break;
            case 4:
                if(write_required) {
                    byte addr = write_required == 2 ? 0x05 : 0x06;
                    write_blocks(addr);
                    if(refresh_step == 0) {
                        ESP_LOGI(
                            TAG,
                            "write error, cycle counter %d: %d",
                            addr,
                            ORIG_BLOCKS[addr][1]
                        );
                        break;
                    }
                    refresh_step --; // repeat the block write step
                    write_required --;
                    ESP_LOGI(
                        TAG,
                        "the number of cycles is written to the cartridge, cycle counter %d: %d",
                        addr,
                        ORIG_BLOCKS[addr][1]
                    );
                }
                break;
            case 5:
                read_blocks(4, 4);
                break;
            case 6:
                read_blocks(8, 4);
                break;
            case 7:
                read_blocks(12, 4);
                break;
            case 8:
                rfid_reset();
                break;
            case 20:
                refresh_step = 0;
                break;
            case 80:
                refresh_step = 0;
                break;
            default:
                break;
        }
        refresh_step++;
        if(millis() - time > 30) {
            ESP_LOGI(TAG, "looks like the i2c bus hangs, %d ms", time - millis());
            refresh_step = 0;
            delay(2);
            Wire1.end();
            delay(2);
            Wire1.begin(I2C1_SDA, I2C1_SCL, I2C1_FREQUENCY);
            Wire1.setTimeOut(10);
            delay(2);
        }
    } else {
        refresh_step = 0;
    }
}

uint8_t CG_RFID::get_remaining_cycles() {
    return is_bypass_active() ? ORIG_BLOCKS[5][1] : BLOCKS[5][1];
}

void CG_RFID::reset() {
    BLOCK_1[1] = 120;
}

void CG_RFID::dump_config() {
    ESP_LOGCONFIG(TAG, "Bypass switch: %d", bpswitch->state);
    ESP_LOGCONFIG(TAG, "Cycle counter: %d", cycle_counter);
}

// Private Methods //////////////////////////////////////////////////////////////

bool CG_RFID::is_bypass_active() {
    return bpswitch->state && ORIG_BLOCKS[15][0] == 0x04;
}

void CG_RFID::check_response(uint8_t err) {
    if(err != 0) {
        ESP_LOGI(TAG, "rfid read error: %d, step: %d", err, refresh_step);
        refresh_step = 0;
    }
}

void CG_RFID::cr14_readframe(byte *data, uint8_t size) {
    uint8_t err = -1;
    uint8_t i = 0;
    for(i = 0; i < 20 && err != 0; i++) {
        Wire1.beginTransmission(I2C_ADDRESS);
        Wire1.write(FRAME);
        err = Wire1.endTransmission();
    }

    if(i == 20) {
        check_response(5);
        return;
    }

    Wire1.requestFrom(I2C_ADDRESS, size);
    if (Wire1.available() >= size) {
        for (int i = 0 ; Wire1.available() ; i++) {       
            data[i] = Wire1.read();
        }
    }
}

void CG_RFID::cr14_writereg(byte value) {
    Wire1.beginTransmission(I2C_ADDRESS);
    Wire1.write(REG);
    Wire1.write(value);
    check_response(Wire1.endTransmission());
}

void CG_RFID::cr14_writeframe(byte *data, uint8_t size) {
    Wire1.beginTransmission(I2C_ADDRESS);
    Wire1.write(FRAME);
    Wire1.write(size);
    Wire1.write(data, size);
    check_response(Wire1.endTransmission());
}

void CG_RFID::rfid_reset() {
    cr14_writereg(0x00);
}

void CG_RFID::read_node_id() {
    cr14_writereg(0x10);
    byte request[] = {READ_NODE, 0x00};
    cr14_writeframe(request, 2);
    cr14_readframe(ORIG_NODE_ID_RESPONSE, 2);
}

void CG_RFID::select_node() {
    byte request[] = {SELECT_NODE, ORIG_NODE_ID_RESPONSE[1]};
    cr14_writeframe(request, 2);
    byte result[] = {0x00, 0x00};
    cr14_readframe(result, 2);
}

void CG_RFID::read_uid() {
    byte request[] = {READ_UID};
    cr14_writeframe(request, 1);
    cr14_readframe(ORIG_UID_RESPONSE, 9);
}

void CG_RFID::read_blocks(uint8_t offset, uint8_t len) {
    for(int i=offset; i<offset+len; i++) {
        byte request[] = {READ_BLOCK, i};
        cr14_writeframe(request, 2);
        cr14_readframe(ORIG_BLOCKS[i], 5);
    }
}

void CG_RFID::write_blocks(byte addr) {
    byte request[] = {
        WRITE_BLOCK,
        addr,
        ORIG_BLOCKS[addr][1],
        ORIG_BLOCKS[addr][2],
        ORIG_BLOCKS[addr][3],
        ORIG_BLOCKS[addr][4]
    };
    cr14_writeframe(request, 6);
}

void CG_RFID::receive_event(int length) {
    if (length == 1) {
        Wire.read();
        return;
    }

    for (int i = 0 ; Wire.available() ; i++) {
        read_register[i] = Wire.read();
    }

    bool bypass = is_bypass_active();
    switch (read_register[2] & 0x0f) {
        case CG_RFID::READ_NODE:
            response = bypass ? ORIG_NODE_ID_RESPONSE : NODE_ID_RESPONSE;
            size = 2;
            break;
        case CG_RFID::READ_BLOCK:
            response = bypass ? ORIG_BLOCKS[read_register[3]] : BLOCKS[read_register[3]];
            size = 5;
            break;
        case CG_RFID::WRITE_BLOCK:
            response = bypass ? ORIG_BLOCKS[read_register[3]] : BLOCKS[read_register[3]];
            response[1] = read_register[4];
            response[2] = read_register[5];
            response[3] = read_register[6];
            response[4] = read_register[7];
            size = 0;

            if(bypass) {
                write_required = 2;
                refresh_step = 21; // wait a minute until all data transfers are completed
            }
            break;
        case CG_RFID::AUTHENTICATE:
            size = 0;
            break;
        case CG_RFID::READ_UID:
            response = bypass ? ORIG_UID_RESPONSE : UID_RESPONSE;
            size = 9;
            break;
        case CG_RFID::RESET_TO_INVENTORY:
            size = 0;
            break;
        case CG_RFID::SELECT_NODE:
            response = bypass ? ORIG_NODE_ID_RESPONSE : NODE_ID_RESPONSE;
            size = 2;
            break;
        case CG_RFID::COMPLETION: 
            size = 0;
            break;
        default:
            break;
    }
}

void CG_RFID::request_event() {
    Wire.write(response, size);
}

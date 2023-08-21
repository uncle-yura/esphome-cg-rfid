#include "esphome.h"

using namespace esphome;
using namespace esphome::switch_;

#ifndef CG_RFID_H
#define CG_RFID_H

static const char *const TAG = "cg_rfid";
//#define DEBUG

namespace esphome {
    class CG_RFID : public PollingComponent {
    public:
        CG_RFID(Switch *switch1) : PollingComponent(1000), bpswitch(switch1) {}
    
        Switch *bpswitch {nullptr};

        void setup() override;
        void update() override;
        void dump_config() override;
        void reset();
    
    private:
        ESPPreferenceObject storage = global_preferences->make_preference<uint8_t>(fnv1_hash(TAG), true);

        uint8_t cycle_counter = 0;

        const int I2C0_SDA = 5;
        const int I2C0_SCL = 3;

        const int I2C1_SDA = 12;
        const int I2C1_SCL = 11;

        const uint32_t I2C_FREQUENCY = 100000;

        const uint8_t I2C_ADDRESS = 64; // 80(7 bits) with a R/W bit appended

        byte* response;
        byte* read_register = new byte[0x10];
        int size = 4;

        byte BLOCK_0[5] = { 0x04, 0x00, 0x00, 0x00, 0x00 };
        byte BLOCK_1[5] = { 0x04, 0x78, 0x00, 0x00, 0x00 };
        byte BLOCK_2[5] = { 0x04, 0x01, 0x00, 0x78, 0x00 };
        byte BLOCK_3[5] = { 0x04, 0x04, 0x0F, 0x00, 0x00 };
        byte BLOCK_4[5] = { 0x04, 0x1F, 0x7E, 0x30, 0x2A };
        byte BLOCK_5[5] = { 0x04, 0x04, 0x0F, 0x2B, 0xFC };

        byte* BLOCKS[16] = {
            BLOCK_0, BLOCK_0, BLOCK_0, BLOCK_0,
            BLOCK_0, BLOCK_1, BLOCK_1, BLOCK_0,
            BLOCK_2, BLOCK_2, BLOCK_2, BLOCK_3,
            BLOCK_3, BLOCK_4, BLOCK_0, BLOCK_5
        };

        byte NODE_ID_RESPONSE[2] = { 0x01, 0x3C };
        byte UID_RESPONSE[9] = { 0x08, 0xA3, 0x6E, 0x6A, 0x73, 0x09, 0x33, 0x02, 0xD0 };
    
        static CG_RFID *instance_; 
        static void handle_request_event() {
            instance_->request_event();
        }

        static void handle_receive_event(int length) {
            instance_->receive_event(length);
        }

        void receive_event(int length);
        void request_event();
    };
}
#endif

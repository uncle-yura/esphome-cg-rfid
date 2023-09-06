#include "esphome.h"

using namespace esphome;
using namespace esphome::switch_;

#ifndef CG_RFID_H
#define CG_RFID_H

static const char *const TAG = "cg_rfid";

namespace esphome {
    class CG_RFID : public PollingComponent {
    public:
        CG_RFID(Switch *switch1) : PollingComponent(1000), bpswitch(switch1) {}
    
        Switch *bpswitch {nullptr};

        void setup() override;
        void update() override;
        void dump_config() override;
        void reset();
        uint8_t get_remaining_cycles();
    
    private:
        ESPPreferenceObject storage = global_preferences->make_preference<uint8_t>(fnv1_hash(TAG), true);

        enum Constants {
            REG = 0x00,
            FRAME = 0x01,
            READ_NODE = 0x06,
            SELECT_NODE = 0x0E,
            READ_UID = 0x0B,
            READ_BLOCK = 0x08,
            WRITE_BLOCK = 0x09,
            AUTHENTICATE = 0x0A,
            RESET_TO_INVENTORY = 0x0C,
            COMPLETION = 0x0F,

            I2C0_SDA = 5,
            I2C0_SCL = 3,
            I2C1_SDA = 12,
            I2C1_SCL = 11,
            I2C0_FREQUENCY = 100000,
            I2C1_FREQUENCY = 100000,
            I2C_ADDRESS = 80
        };    

        uint8_t refresh_step = 0;
        bool write_required = false;
        uint8_t cycle_counter = 0;

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
    
        byte ORIG_BLOCKS[16][5];
        byte ORIG_NODE_ID_RESPONSE[2];
        byte ORIG_UID_RESPONSE[9];
    
        static CG_RFID *instance_; 
        static void handle_request_event() {
            instance_->request_event();
        }

        static void handle_receive_event(int length) {
            instance_->receive_event(length);
        }

        bool is_bypass_active();
        void receive_event(int length);
        void request_event();
        void cr14_writereg(byte value);
        void cr14_readframe(byte *data, uint8_t size);
        void cr14_writeframe(byte *data, uint8_t size);
        void rfid_reset();
        void read_node_id();
        void select_node();
        void read_uid();
        void read_blocks(uint8_t offset, uint8_t len);
        void write_blocks();
        void check_response(uint8_t err);
    };
}
#endif

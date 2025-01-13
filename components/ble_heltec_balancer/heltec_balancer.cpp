#include "heltec_balancer.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome
{
    namespace ble_heltec_balancer
    {
        static const char *const TAG = "ble_heltec_balancer";

        static const uint8_t MAX_NO_RESPONSE_COUNT = 10;

        static const uint16_t HELTEC_BALANCER_SERVICE_UUID = 0xFFE0;
        static const uint16_t HELTEC_BALANCER_CHARACTERISTIC_UUID = 0xFFE1;

        static const uint8_t SOF_REQUEST_BYTE1 = 0xAA;
        static const uint8_t SOF_REQUEST_BYTE2 = 0x55;
        static const uint8_t SOF_RESPONSE_BYTE1 = 0x55;
        static const uint8_t SOF_RESPONSE_BYTE2 = 0xAA;
        static const uint8_t DEVICE_ADDRESS = 0x11;

        static const uint8_t FUNCTION_WRITE = 0x00;
        static const uint8_t FUNCTION_READ = 0x01;

        static const uint8_t COMMAND_NONE = 0x00;
        static const uint8_t COMMAND_DEVICE_INFO = 0x01;
        static const uint8_t COMMAND_CELL_INFO = 0x02;
        static const uint8_t COMMAND_FACTORY_DEFAULTS = 0x03;
        static const uint8_t COMMAND_SETTINGS = 0x04;
        static const uint8_t COMMAND_WRITE_REGISTER = 0x05;

        static const uint8_t END_OF_FRAME = 0xFF;

        static const uint16_t MIN_RESPONSE_SIZE = 20;   // Write acknowledge frame
        static const uint16_t MAX_RESPONSE_SIZE = 300;  // Cell info frame

        static const uint8_t OPERATION_STATUS_SIZE = 13;
        static const char *const OPERATION_STATUS[OPERATION_STATUS_SIZE] =
        {
            "Unknown",                                   // 0x00
            "Wrong cell count",                          // 0x01
            "AcqLine Res test",                          // 0x02
            "AcqLine Res exceed",                        // 0x03
            "Systest Completed",                         // 0x04
            "Balancing",                                 // 0x05
            "Balancing finished",                        // 0x06
            "Low voltage",                               // 0x07
            "System Overtemp",                           // 0x08
            "Host fails",                                // 0x09
            "Low battery voltage - balancing stopped",   // 0x0A
            "Temperature too high - balancing stopped",  // 0x0B
            "Self-test completed",                       // 0x0C
        };

        static const uint8_t BUZZER_MODES_SIZE = 4;
        static const char *const BUZZER_MODES[BUZZER_MODES_SIZE] =
        {
            "Unknown",       // 0x00
            "Off",           // 0x01
            "Beep once",     // 0x02
            "Beep regular",  // 0x03
        };

        static const uint8_t BATTERY_TYPES_SIZE = 5;
        static const char *const BATTERY_TYPES[BATTERY_TYPES_SIZE] =
        {
            "Unknown",  // 0x00
            "NCM",      // 0x01
            "LFP",      // 0x02
            "LTO",      // 0x03
            "PbAc",     // 0x04
        };

        static const uint8_t CELL_ERRORS_SIZE = 8;
        static const char *const CELL_ERRORS[CELL_ERRORS_SIZE] =
        {
            "Battery detection failed",
            "Overvoltage",
            "Undervoltage",
            "Polarity error",
            "Excessive line resistance",
            "System overheating",
            "Charging fault",
            "Discharge fault",
        };

        uint8_t crc(const uint8_t data[], const uint16_t len)
        {
            uint8_t crc = 0;
            for (uint16_t i = 0; i < len; i++)
            {
                crc = crc + data[i];
            }
            return crc;
        }

        const HeltecBalancerBle::StringRef& get_name() const
        {
            static StringRef name_ = StringRef("Test");
            return name_;
        }

        bool HeltecBalancerBle::send_command(uint8_t function, uint8_t command, uint8_t register_address, uint32_t value)
        {
          // Request device info:
          //
          // (GW-24S4EB, checksum_xor)
          // 0xAA 0x55 0x11 0x01 0x01 0x00 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFA 0xFF
          // 0xAA 0x55 0x11 0x01 0x01 0x00 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x26 0xFF
          // (EK-24S4EB, crc)
          //
          // Request cell info:
          //
          // (GW-24S4EB, checksum_xor, wrong data_len position)
          // 0xAA 0x55 0x11 0x01 0x02 0x00 0x00 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xF9 0xFF
          // 0xAA 0x55 0x11 0x01 0x02 0x00 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x27 0xFF
          // (EK-24S4EB, crc)
          //
          // Request factory settings:
          //
          // (GW-24S4EB, checksum_xor)
          // 0xAA 0x55 0x11 0x01 0x03 0x00 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xF8 0xFF
          //
          // Request settings:
          //
          // (GW-24S4EB, checksum_xor)
          // 0xAA 0x55 0x11 0x01 0x04 0x00 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xFF 0xFF
          // 0xAA 0x55 0x11 0x01 0x04 0x00 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x29 0xFF
          // (EK-24S4EB, crc)
          //
          // Enable balancer:
          //
          // (GW-24S4EB, checksum_xor)
          // 0xAA 0x55 0x11 0x00 0x05 0x0D 0x14 0x00 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xF3 0xFF
          //
          // Disable balancer:
          //
          // (GW-24S4EB, checksum_xor)
          // 0xAA 0x55 0x11 0x00 0x05 0x0D 0x14 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0xF2 0xFF
          uint16_t length = 0x0014;

          uint8_t frame[20];
          frame[0] = SOF_REQUEST_BYTE1;  // Start sequence
          frame[1] = SOF_REQUEST_BYTE2;  // Start sequence
          frame[2] = DEVICE_ADDRESS;     // Device address
          frame[3] = function;           // Function (read or write)
          frame[4] = command >> 0;       // Command
          frame[5] = register_address;   // Register address
          frame[6] = length >> 0;        // Data length
          frame[7] = length >> 8;        // Data length
          frame[8] = value >> 0;         // Data Byte 1
          frame[9] = value >> 8;         // Data Byte 2
          frame[10] = value >> 16;       // Data Byte 3
          frame[11] = value >> 24;       // Data Byte 4
          frame[12] = 0x00;              // Data Byte 5
          frame[13] = 0x00;              // Data Byte 6
          frame[14] = 0x00;              // Data Byte 7
          frame[15] = 0x00;              // Data Byte 8
          frame[16] = 0x00;              // Data Byte 9
          frame[17] = 0x00;              // Data Byte 10
          frame[18] = crc(frame, sizeof(frame) - 2);
          frame[19] = END_OF_FRAME;  // End sequence

          ESP_LOGD(TAG, "Write register: %s", format_hex_pretty(frame, sizeof(frame)).c_str());
          auto status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->handle, sizeof(frame), frame, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);

          if (status)
            ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(), status);

          return (status == 0);
        }

        void HeltecBalancerBle::loop()
        {

        }

        void HeltecBalancerBle::dump_config()
        {
            ESP_LOGCONFIG(TAG, "HeltecBalancerBle");
            ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent()->address_str().c_str());
            LOG_UPDATE_INTERVAL(this);
        }

        void HeltecBalancerBle::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
        {
            switch (event)
            {
            case ESP_GATTC_CLOSE_EVT:
            {
                this->status_set_warning();
                //this->publish_state(NAN);

                break;
            }
            case ESP_GATTC_SEARCH_CMPL_EVT:
            {
                this->handle = 0;
                auto *chr = this->parent()->get_characteristic(HELTEC_BALANCER_SERVICE_UUID, HELTEC_BALANCER_CHARACTERISTIC_UUID);
                if (chr == nullptr)
                {
                    this->status_set_warning();
                    //this->publish_state(NAN);
                    ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", HELTEC_BALANCER_SERVICE_UUID, HELTEC_BALANCER_CHARACTERISTIC_UUID);
                    break;
                }

                this->handle = chr->handle;

                //if (this->descr_uuid_.get_uuid().len > 0)
                //{
                //    auto *descr = chr->get_descriptor(this->descr_uuid_);
                //
                //    if (descr == nullptr)
                //    {
                //        this->status_set_warning();
                //        this->publish_state(NAN);
                //        ESP_LOGW(TAG, "No sensor descriptor found at service %s char %s descr %s",
                //                this->service_uuid_.to_string().c_str(), this->char_uuid_.to_string().c_str(),
                //                this->descr_uuid_.to_string().c_str());
                //        break;
                //    }
                //
                //    this->handle = descr->handle;
                //}

                if (true)
                {
                    auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), chr->handle);
                    if (status)
                    {
                        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
                    }
                }
                else
                {
                    this->node_state = espbt::ClientState::ESTABLISHED;
                }
                break;
            }
            //case ESP_GATTC_SEARCH_CMPL_EVT:
            //{
            //    this->node_state = espbt::ClientState::ESTABLISHED;
            //
            //    if (this->should_update_)
            //    {
            //        this->should_update_ = false;
            //        this->get_data_();
            //    }
            //
            //    break;
            //}
            default:
                break;
          }
        }

        void HeltecBalancerBle::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
        {
          ESP_LOGI(TAG, "ESP_GAP_BLE: %d", event);
          //switch (event)
          //{
          //  // server response on RSSI request:
          //  case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
          //  {
          //      if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS)
          //      {
          //          int8_t rssi = param->read_rssi_cmpl.rssi;
          //          ESP_LOGI(TAG, "ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT RSSI: %d", rssi);
          //          this->status_clear_warning();
          //          this->publish_state(rssi);
          //      }
          //
          //      break;
          //  }
          //  default:
          //      break;
          //}
        }

        void HeltecBalancerBle::update()
        {
            if (this->node_state != espbt::ClientState::ESTABLISHED)
            {
                //ESP_LOGW(TAG, "[%s] Cannot poll, not connected", this->get_name().c_str());
                this->should_update_ = true;
                return;
            }

            this->get_data_();
        }

        void HeltecBalancerBle::get_data_()
        {
            if (this->node_state != espbt::ClientState::ESTABLISHED)
            {
                //ESP_LOGW(TAG, "[%s] Cannot poll, not connected", this->get_name().c_str());

                return;
            }
            
            if (this->handle == 0)
            {
                //ESP_LOGW(TAG, "[%s] Cannot poll, no service or characteristic found", this->get_name().c_str());

                return;
            }

            auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->handle, ESP_GATT_AUTH_REQ_NONE);

            if (status)
            {
                this->status_set_warning();
                //this->publish_state(NAN);
                //ESP_LOGW(TAG, "[%s] Error sending read request for sensor, status=%d", this->get_name().c_str(), status);
            }
            
            //ESP_LOGV(TAG, "Request device info from %s", this->parent()->address_str().c_str());
            //send_command(FUNCTION_READ, COMMAND_DEVICE_INFO);
            
            //ESP_LOGV(TAG, "requesting rssi from %s", this->parent()->address_str().c_str());
            //auto status = esp_ble_gap_read_rssi(this->parent()->get_remote_bda());
            //if (status != ESP_OK)
            //{
            //    ESP_LOGW(TAG, "esp_ble_gap_read_rssi error, address=%s, status=%d", this->parent()->address_str().c_str(), status);
            //    this->status_set_warning();
            //    this->publish_state(NAN);
            //}
        }

    }  // namespace ble_client
}  // namespace esphome
#endif
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/sensor/sensor.h"


#ifdef USE_ESP32
#include <esp_gattc_api.h>

namespace esphome
{
    namespace ble_heltec_balancer
    {
        namespace espbt = esphome::esp32_ble_tracker;

        class HeltecBalancerBle : public esphome::ble_client::BLEClientNode, public PollingComponent
        {
        public:
            float get_setup_priority() const override;

            void loop() override;
            void update() override;
            void dump_config() override;

            void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
            void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) override;

            //void set_cell_sensor(uint8_t cell_num, sensor::Sensor *Cell_Sensor);
            //void set_summary_cells_sensor(uint8_t summary_type, sensor::Sensor *Cell_Sensor);
            //void set_temperature_sensor(uint8_t temperature_num, sensor::Sensor *Temperature_Sensor);

        protected:
            void get_data_();
            bool should_update_ = false;

        //protected:
		//	bool init_ble_state = false;
		//	void init_ble();
        //    bool init_balancer_state = false;
		//	void init_balancer();
        //
        //    sensor::Sensor** Cell_Sensors = nullptr;
        //    sensor::Sensor** Summary_Cell_Sensors = nullptr;
        //    sensor::Sensor** Temperature_Sensors = nullptr;
        };
    }  // namespace ble_client
}  // namespace esphome
#endif
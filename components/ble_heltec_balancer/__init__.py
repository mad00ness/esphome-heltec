from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble_tracker, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

from .. import ble_client_ns

DEPENDENCIES = ["ble_client"]

TYPE_BALANCER = "balancer"

CONF_HELTEC_BALANCER_BLE_ID = "heltec_balancer_ble_id"

heltec_balancer_ble_ns = cg.esphome_ns.namespace("ble_heltec_balancer")
HeltecBalancerBle = heltec_balancer_ble_ns.class_(
    "HeltecBalancerBle", ble_client.BLEClientNode, cg.PollingComponent
)

HELTEC_BALANCER_BLE_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELTEC_BALANCER_BLE_ID): cv.use_id(HeltecBalancerBle),
    }
)

CONFIG_SCHEMA = cv.All(
    checkType,
    cv.typed_schema(
        {
            TYPE_BALANCER: sensor.sensor_schema(
                HeltecBalancerBle,
                accuracy_decimals=0,
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            .extend(cv.polling_component_schema("60s"))
            .extend(ble_client.BLE_CLIENT_SCHEMA),
        },
        lower=True,
    ),
)

async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

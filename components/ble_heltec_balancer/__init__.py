import esphome.codegen as cg
from esphome.components import ble_client
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["ble_client"]

MULTI_CONF = True

CONF_BLE_HELTEC_BALANCER_ID = "ble_heltec_balancer_id"

heltec_balancer_ble_ns = cg.esphome_ns.namespace("ble_heltec_balancer")
HeltecBalancerBle = heltec_balancer_ble_ns.class_(
    "HeltecBalancerBle", ble_client.BLEClientNode, cg.PollingComponent
)

HELTEC_BALANCER_BLE_COMPONENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLE_HELTEC_BALANCER_ID): cv.use_id(HeltecBalancerBle),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HeltecBalancerBle),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("5s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)


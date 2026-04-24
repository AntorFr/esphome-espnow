import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_ID

CODEOWNERS = ["@esphome-espnow"]

DEPENDENCIES = ["wifi"]

espnow_receiver_ns = cg.esphome_ns.namespace("espnow_receiver")
ESPNowReceiverComponent = espnow_receiver_ns.class_(
    "ESPNowReceiverComponent", cg.Component
)

CONF_PEER_MAC = "peer_mac"
CONF_LIGHT_ID = "light_id"


def validate_mac(value):
    value = cv.string(value)
    parts = value.split(":")
    if len(parts) != 6:
        raise cv.Invalid("MAC address must have 6 parts separated by ':'")
    for part in parts:
        if len(part) != 2:
            raise cv.Invalid("Each part of a MAC address must be exactly 2 hex digits")
        try:
            int(part, 16)
        except ValueError:
            raise cv.Invalid(f"Invalid hex value '{part}' in MAC address")
    return value.upper()


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESPNowReceiverComponent),
        cv.Required(CONF_PEER_MAC): validate_mac,
        cv.Required(CONF_LIGHT_ID): cv.use_id(light.LightState),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mac_str = config[CONF_PEER_MAC]
    mac_parts = [int(x, 16) for x in mac_str.split(":")]
    cg.add(var.set_peer_mac(mac_parts))

    light_var = await cg.get_variable(config[CONF_LIGHT_ID])
    cg.add(var.set_light(light_var))

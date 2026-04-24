import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID

CODEOWNERS = ["@esphome-espnow"]

DEPENDENCIES = ["wifi"]

espnow_sender_ns = cg.esphome_ns.namespace("espnow_sender")
ESPNowSenderComponent = espnow_sender_ns.class_(
    "ESPNowSenderComponent", cg.Component
)

CONF_PEER_MAC = "peer_mac"
CONF_FALLBACK_TIMEOUT = "fallback_timeout"
CONF_RECOVERY_TIMEOUT = "recovery_timeout"
CONF_ACTION = "action"
CONF_BINARY_SENSOR_ID = "binary_sensor_id"

ESPNowSendAction = espnow_sender_ns.enum("ESPNowSendAction")
ACTION_OPTIONS = {
    "toggle": ESPNowSendAction.ACTION_TOGGLE,
    "on": ESPNowSendAction.ACTION_ON,
    "off": ESPNowSendAction.ACTION_OFF,
    "mirror_state": ESPNowSendAction.ACTION_MIRROR_STATE,
}


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
        cv.GenerateID(): cv.declare_id(ESPNowSenderComponent),
        cv.Required(CONF_PEER_MAC): validate_mac,
        cv.Optional(CONF_FALLBACK_TIMEOUT, default="30s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RECOVERY_TIMEOUT, default="10s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ACTION, default="toggle"): cv.enum(ACTION_OPTIONS, lower=True),
        cv.Optional(CONF_BINARY_SENSOR_ID): cv.use_id(binary_sensor.BinarySensor),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mac_str = config[CONF_PEER_MAC]
    mac_parts = [int(x, 16) for x in mac_str.split(":")]
    cg.add(var.set_peer_mac(mac_parts))

    cg.add(var.set_fallback_timeout(config[CONF_FALLBACK_TIMEOUT]))
    cg.add(var.set_recovery_timeout(config[CONF_RECOVERY_TIMEOUT]))
    cg.add(var.set_action(config[CONF_ACTION]))

    if CONF_BINARY_SENSOR_ID in config:
        bs_var = await cg.get_variable(config[CONF_BINARY_SENSOR_ID])
        cg.add(var.set_binary_sensor(bs_var))

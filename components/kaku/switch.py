import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from . import CONF_ADDRESS, CONF_UNIT, CONF_GROUP, kaku_ns, KakuProtocol

DEPENDENCIES = ["kaku", "remote_transmitter"]

KakuSwitch = kaku_ns.class_("KakuSwitch", switch.Switch, cg.Component)

CONF_TRANSMITTER_ID = "transmitter_id"
CONF_REPEATS = "repeats"

from esphome.components.remote_transmitter import RemoteTransmitterComponent

CONFIG_SCHEMA = (
    switch.switch_schema(KakuSwitch)
    .extend(
        {
            cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(RemoteTransmitterComponent),
            cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
            cv.Optional(CONF_UNIT, default=0): cv.int_range(min=0, max=15),
            cv.Optional(CONF_GROUP, default=False): cv.boolean,
            cv.Optional(CONF_REPEATS, default=8): cv.int_range(min=1, max=50),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    transmitter = await cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_unit(config[CONF_UNIT]))
    cg.add(var.set_group(config[CONF_GROUP]))
    cg.add(var.set_repeats(config[CONF_REPEATS]))

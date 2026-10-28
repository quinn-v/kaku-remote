import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_base, switch
from esphome.const import CONF_ID

from . import CONF_ADDRESS, CONF_GROUP, CONF_UNIT, KakuProtocol, kaku_ns

DEPENDENCIES = ["kaku"]

KakuSwitch = kaku_ns.class_(
    "KakuSwitch", switch.Switch, cg.Component, remote_base.RemoteTransmittable
)

CONF_REPEATS = "repeats"

CONFIG_SCHEMA = (
    switch.switch_schema(KakuSwitch)
    .extend(remote_base.BASE_REMOTE_TRANSMITTER_SCHEMA)
    .extend(
        {
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
    await remote_base.register_transmittable(var, config)
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_unit(config[CONF_UNIT]))
    cg.add(var.set_group(config[CONF_GROUP]))
    cg.add(var.set_repeats(config[CONF_REPEATS]))

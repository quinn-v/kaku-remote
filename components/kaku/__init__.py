import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_base
from esphome.const import CONF_ID

CODEOWNERS = ["@quinn-v"]
DEPENDENCIES = ["remote_base"]

kaku_ns = cg.esphome_ns.namespace("kaku")
KakuData = kaku_ns.struct("KakuData")
KakuProtocol = kaku_ns.class_("KakuProtocol")

CONF_ADDRESS = "address"
CONF_UNIT = "unit"
CONF_STATE = "state"
CONF_GROUP = "group"
CONF_DIM_LEVEL = "dim_level"

KAKU_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
        cv.Optional(CONF_UNIT, default=0): cv.int_range(min=0, max=15),
        cv.Optional(CONF_GROUP, default=False): cv.boolean,
        cv.Optional(CONF_STATE, default=True): cv.boolean,
        cv.Optional(CONF_DIM_LEVEL): cv.int_range(min=0, max=15),
    }
)

KakuBinarySensor = kaku_ns.class_("KakuBinarySensor", remote_base.RemoteReceiverBinarySensorBase)
KakuTrigger = kaku_ns.class_("KakuTrigger", remote_base.RemoteReceiverTrigger)
KakuAction = kaku_ns.class_("KakuAction", remote_base.RemoteTransmitterActionBase)
KakuDumper = kaku_ns.class_("KakuDumper", remote_base.RemoteReceiverDumperBase)

@remote_base.register_binary_sensor("kaku", KakuBinarySensor, KAKU_SCHEMA)
def kaku_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                KakuData,
                ("address", config[CONF_ADDRESS]),
                ("unit", config[CONF_UNIT]),
                ("group", config[CONF_GROUP]),
                ("state", config[CONF_STATE]),
                ("dim_level", config.get(CONF_DIM_LEVEL, 0)),
            )
        )
    )

@remote_base.register_trigger("kaku", KakuTrigger, KakuData)
def kaku_trigger(var, config):
    pass

@remote_base.register_dumper("kaku", KakuDumper)
def kaku_dumper(var, config):
    pass

@remote_base.register_action("kaku", KakuAction, KAKU_SCHEMA)
async def kaku_action(var, config, args):
    cg.add(var.set_address(await cg.templatable(config[CONF_ADDRESS], args, cg.uint32)))
    cg.add(var.set_unit(await cg.templatable(config[CONF_UNIT], args, cg.uint8)))
    cg.add(var.set_group(await cg.templatable(config[CONF_GROUP], args, cg.bool_)))
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, cg.bool_)))
    if CONF_DIM_LEVEL in config:
        cg.add(var.set_dim_level(await cg.templatable(config[CONF_DIM_LEVEL], args, cg.uint8)))
        cg.add(var.set_is_dim(True))
    else:
        cg.add(var.set_is_dim(False))

CONFIG_SCHEMA = cv.Schema({})

async def to_code(config):
    pass

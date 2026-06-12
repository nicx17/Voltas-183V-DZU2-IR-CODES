import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["remote_transmitter"]

voltas_ac_ns = cg.esphome_ns.namespace("voltas_ac")

"""Light platform for Fastcon BLE lights."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_LIGHT_ID, CONF_OUTPUT_ID
from .fastcon_controller import FastconController

# New config key to toggle RGBCW capability per-entity
CONF_SUPPORTS_CWWW = "supports_cwww"

DEPENDENCIES = ["esp32_ble"]
AUTO_LOAD = ["light"]

CONF_CONTROLLER_ID = "controller_id"
CONF_GROUP_ID = "group_id"  # New configuration key for groups

fastcon_ns = cg.esphome_ns.namespace("fastcon")
FastconLight = fastcon_ns.class_("FastconLight", light.LightOutput, cg.Component)

CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA
    .extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FastconLight),
            # Changed from Required to Optional for light_id
            cv.Optional(CONF_LIGHT_ID): cv.int_range(min=1, max=255),
            # New optional group_id parameter
            cv.Optional(CONF_GROUP_ID): cv.int_range(min=1, max=255),
            cv.Optional(CONF_CONTROLLER_ID, default="fastcon_controller"): cv.use_id(FastconController),
            cv.Optional(CONF_SUPPORTS_CWWW, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    # VALIDATION: Must have either light_id OR group_id
    cv.has_at_least_one_key(CONF_LIGHT_ID, CONF_GROUP_ID)
)

async def to_code(config):
    # FIX: Create without initial light_id parameter    
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    #var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_LIGHT_ID])

    await cg.register_component(var, config)
    await light.register_light(var, config)

    # Assign the appropriate ID (light_id or group_id)
    if CONF_LIGHT_ID in config:
        light_id = config[CONF_LIGHT_ID]
        cg.add(var.set_light_id(light_id))
    elif CONF_GROUP_ID in config:
        group_id = config[CONF_GROUP_ID]
        # This calls the new set_group_id() method in C++
        cg.add(var.set_group_id(group_id))


    controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
    cg.add(var.set_controller(controller))

    if config.get(CONF_SUPPORTS_CWWW):
        cg.add(var.set_supports_cwww(True))
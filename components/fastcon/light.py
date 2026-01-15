"""Light platform for Fastcon BLE lights."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_LIGHT_ID, CONF_OUTPUT_ID, CONF_ID
from .fastcon_controller import FastconController

# New config key to toggle RGBCW capability per-entity
CONF_SUPPORTS_CWWW = "supports_cwww"

DEPENDENCIES = ["esp32_ble"]
AUTO_LOAD = ["light"]

CONF_CONTROLLER_ID = "controller_id"
CONF_GROUP_ID = "group_id"  # New configuration key for groups
CONF_MEMBERS_WITH_ID = "members_with_id"


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
#            cv.Optional(CONF_MEMBERS): cv.ensure_list(cv.use_id(light.LightState)),
            cv.Optional(CONF_MEMBERS_WITH_ID): cv.ensure_list(
                cv.Schema({
                    cv.Required(CONF_ID): cv.use_id(light.LightState),
                    cv.Required(CONF_LIGHT_ID): cv.int_range(min=1, max=255),
                })
            ),            
            cv.Optional(CONF_CONTROLLER_ID, default="fastcon_controller"): cv.use_id(FastconController),
            cv.Optional(CONF_SUPPORTS_CWWW, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    # VALIDATION: Must have either light_id OR group_id
    cv.has_at_least_one_key(CONF_LIGHT_ID, CONF_GROUP_ID)
)

async def to_code(config):
    light_id_value = config.get(CONF_LIGHT_ID, 0)
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], light_id_value)

    await cg.register_component(var, config)
    await light.register_light(var, config)

    # Assign the appropriate ID (light_id or group_id)
    if CONF_LIGHT_ID in config:
        light_id = config[CONF_LIGHT_ID]
        cg.add(var.set_light_id(light_id))
    elif CONF_GROUP_ID in config:
        group_id = config[CONF_GROUP_ID]
        cg.add(var.set_group_id(group_id))

    # Assign controller
    controller = await cg.get_variable(config.get(CONF_CONTROLLER_ID, "fastcon_controller"))
    cg.add(var.set_controller(controller))

    # Assign the appropriate ID (light_id or group_id)
    if CONF_LIGHT_ID in config:
        light_id = config[CONF_LIGHT_ID]
        cg.add(var.set_light_id(light_id))
    elif CONF_GROUP_ID in config:
        group_id = config[CONF_GROUP_ID]
        cg.add(var.set_group_id(group_id))

    # Assign controller
    controller = await cg.get_variable(config.get(CONF_CONTROLLER_ID, "fastcon_controller"))
    cg.add(var.set_controller(controller))

    # Assign members (convert Python ID -> C++ pointer + light_id)
    # ─────────────────────────────
    # REGISTRE DE GRUPS
    # ─────────────────────────────
    if CONF_MEMBERS_WITH_ID in config:
        controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
        for m in config[CONF_MEMBERS_WITH_ID]:
            member_state = await cg.get_variable(m[CONF_ID])
            member_light_id = m[CONF_LIGHT_ID]

            cg.add(controller.register_group_member(
                member_light_id,
                config[CONF_GROUP_ID],
                member_state
            ))


    # Supports CWWW?
    if config.get(CONF_SUPPORTS_CWWW):
        cg.add(var.set_supports_cwww(True))

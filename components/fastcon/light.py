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

# Variables globals
FASTCON_GROUPS = {}
FASTCON_GROUP_STATES = {}

fastcon_ns = cg.esphome_ns.namespace("fastcon")
FastconLight = fastcon_ns.class_("FastconLight", light.LightOutput, cg.Component)

# OPCIÓ 1: Funció finalize SIMPLIFICADA (recomanada)
def finalize_fastcon_groups(config):
    """Validació final que registra tots els grups"""
    # Aquesta funció s'executa durant la validació
    async def generate_all_groups():
        if not FASTCON_GROUPS:
            return
        
        controller_id = config.get(CONF_CONTROLLER_ID, "fastcon_controller")
        controller = await cg.get_variable(controller_id)
        
        for group_id, members in FASTCON_GROUPS.items():
            group_state = FASTCON_GROUP_STATES.get(group_id)
            
            for light_id, member_id in members:
                member_state = await cg.get_variable(member_id)
                
                cg.add(
                    controller.register_group_member(
                        light_id,
                        group_id,
                        member_state,
                        group_state if group_state else cg.RawExpression("nullptr")
                    )
                )
    
    # Afegir a la cua de compilació
    cg.add_to_queue(generate_all_groups())
    
    # Retornar config sense canvis
    return config

# OPCIÓ 2: Usar cv.register_final_validate (més proper a l'estàndard)
# Com que cv.register_final_validate pot ser complex, anem amb l'Opció 1

CONFIG_SCHEMA = cv.All(
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA
    .extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FastconLight),
            cv.Optional(CONF_LIGHT_ID): cv.int_range(min=1, max=255),
            cv.Optional(CONF_GROUP_ID): cv.int_range(min=1, max=255),
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
    cv.has_at_least_one_key(CONF_LIGHT_ID, CONF_GROUP_ID)
    # NOTA: finalize_fastcon_groups NO es posa aquí!
)

# En lloc d'usar cv.All, afegim el finalize DIRECTAMENT a la definició del component
# Modifiquem async def to_code per cridar finalize_fastcon_groups

async def to_code(config):
    light_id_value = config.get(CONF_LIGHT_ID, 0)
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], light_id_value)  # FastconLight*

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

    # Registrar LightState del grup (si és un grup)
    if CONF_GROUP_ID in config:
        group_id = config[CONF_GROUP_ID]
        group_state = await cg.get_variable(config[CONF_ID])
        FASTCON_GROUP_STATES[group_id] = group_state

    # Registrar membres
    if CONF_MEMBERS_WITH_ID in config:
        group_id = config[CONF_GROUP_ID]
        if group_id not in FASTCON_GROUPS:
            FASTCON_GROUPS[group_id] = []
        for m in config[CONF_MEMBERS_WITH_ID]:
            FASTCON_GROUPS[group_id].append(
                (m[CONF_LIGHT_ID], m[CONF_ID])
            )
    
    # 🔥 AQUESTA ÉS LA CLAU: Cridar finalize_fastcon_groups
    # Però només un cop! Afegim un flag global
    if not hasattr(to_code, "_finalized"):
        # Cridar la funció de finalització
        finalize_fastcon_groups(config)
        to_code._finalized = True
    
    # Supports CWWW?
    if config.get(CONF_SUPPORTS_CWWW):
        cg.add(var.set_supports_cwww(True))
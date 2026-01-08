#pragma once

#include <array>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "fastcon_controller.h"

namespace esphome
{
    namespace fastcon
    {
        enum class LightState
        {
            OFF,
            WARM_WHITE,
            RGB
        };

        class FastconLight : public Component, public light::LightOutput
        {
        public:
            void set_light_id(uint32_t light_id) { light_id_ = light_id; is_group_ = false; }
            void set_group_id(uint32_t group_id) { light_id_ = group_id; is_group_ = true; } // <-- New - group ID            

            FastconLight(uint8_t light_id) : light_id_(light_id) {}
    
            void setup() override;
            light::LightTraits get_traits() override;
            void write_state(light::LightState *state) override;
            void set_controller(FastconController *controller);

        protected:
            FastconController *controller_{nullptr};
            uint8_t light_id_;
            bool is_group_{false};  // <-- NEW - Indicate this is a group
        };
    } // namespace fastcon
} // namespace esphome

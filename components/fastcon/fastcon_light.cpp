#include <algorithm>
#include "esphome/core/log.h"
#include "fastcon_light.h"
#include "fastcon_controller.h"
#include "utils.h"

namespace esphome
{
    namespace fastcon
    {
        static const char *const TAG = "fastcon.light";

        void FastconLight::setup()
        {
            if (this->controller_ == nullptr)
            {
                ESP_LOGE(TAG, "Controller not set for light %d!", this->light_id_);
                this->mark_failed();
                return;
            }
            
            ESP_LOGCONFIG(TAG, "Setting up Fastcon BLE light (ID: %d)...", this->light_id_);
            
            // Determinar si és grup o llum individual
            bool is_group = is_group_;
            bool is_individual = (light_id_ > 0);
            
            ESP_LOGCONFIG(TAG, "  Fastcon Light: %s", 
                is_group ? "Grup" : (is_individual ? "Individual" : "Unknown"));
            
            if (is_group) {
                ESP_LOGCONFIG(TAG, "    Group ID: %u", light_id_);
            }
            if (is_individual) {
                ESP_LOGCONFIG(TAG, "    Light ID: %u", light_id_);
            }
            
            // 🔥 AQUESTA ÉS LA CLAU: Auto-registrar als grups pendents
            if (is_individual && state_) {
                ESP_LOGD(TAG, "Auto-registering light_id %u to pending groups", light_id_);
                controller_->auto_register_to_groups(light_id_, state_);
            }
            
            // Si és un grup, podríem registrar el seu LightState aquí
            if (is_group && state_) {
                ESP_LOGD(TAG, "Setting up group %u with LightState %p", light_id_, state_);
                // Opcional: registrar el group_state al controlador
                // controller_->register_group_state(group_id_, state_);
            }
        }


        void FastconLight::set_controller(FastconController *controller)
        {
            this->controller_ = controller;
        }

        light::LightTraits FastconLight::get_traits()
        {
            auto traits = light::LightTraits();
            traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::WHITE, light::ColorMode::BRIGHTNESS, light::ColorMode::COLD_WARM_WHITE});
            traits.set_min_mireds(153);
            traits.set_max_mireds(500);
            return traits;
        }

        void FastconLight::write_state(light::LightState *state) {
            auto &vals = state->current_values;

            auto light_data = this->controller_->get_light_data(state);

            auto adv_data =
                this->controller_->single_control(this->light_id_, light_data, this->is_group_);

            this->controller_->queueCommand(this->light_id_, adv_data);

            if (this->controller_ != nullptr) {
              this->controller_->on_state_changed(this->light_id_, state, this->is_group_);
              this->controller_->dump_groups();
            }            

/*   es mou a controller         
            // 🔥 SINCRONITZACIÓ DELS MEMBRES
            if (this->is_group_) {
                for (auto *member : this->group_members_) {
                if (member == nullptr)
                    continue;

                ESP_LOGD(TAG, "Sync member LightState %p from group %d",
                        member);

                auto call = member->make_call();

                call.set_state(vals.is_on());
                call.set_brightness(vals.get_brightness());

                if (vals.get_color_mode() == light::ColorMode::RGB) {
                    call.set_rgb(vals.get_red(), vals.get_green(), vals.get_blue());
                }

                if (vals.get_color_mode() == light::ColorMode::COLOR_TEMPERATURE) {
                    call.set_color_temperature(vals.get_color_temperature());
                }

                call.perform();
                }
            }
*/
        }


 /*
        void FastconLight::write_state2(light::LightState *state)
        {
            // Get the light data bits from the state
            auto light_data = this->controller_->get_light_data(state);

            // Debug output - print the light state values
            bool is_on = (light_data[0] & 0x80) != 0;
            float brightness = ((light_data[0] & 0x7F) / 127.0f) * 100.0f;
            if (light_data.size() == 1)
            {
                ESP_LOGD(TAG, "Writing state: light_id=%d, on=%d, brightness=%.1f%%", light_id_, is_on, brightness);
            }
            else
            {
                auto r = light_data[2];
                auto g = light_data[3];
                auto b = light_data[1];
                auto warm = light_data[4];
                auto cold = light_data[5];
                ESP_LOGD(TAG, "Writing state: light_id=%d, on=%d, brightness=%.1f%%, rgb=(%d,%d,%d), warm=%d, cold=%d", light_id_, is_on, brightness, r, g, b, warm, cold);
            }

            // Generate the advertisement payload
            auto adv_data = this->controller_->single_control(this->light_id_, light_data, this->is_group_);

            // Debug output - print payload as hex
            auto hex_str = vector_to_hex_string(adv_data).data();
            ESP_LOGD(TAG, "Advertisement Payload (%d bytes): %s", adv_data.size(), hex_str);

            // Send the advertisement
            this->controller_->queueCommand(this->light_id_, adv_data);

            if (is_group_) {
                for (auto member : group_members_) {
                    if (member != nullptr) {
                        auto call = member->make_call();
                        
                        // Estat i brillantor
                        call.set_state(is_on);
                        call.set_brightness(brightness);

                        if (member->current_values.supports_rgb()) {
                            auto rgb = state->current_values.get_rgb();
                            call.set_rgb(rgb.red, rgb.green, rgb.blue);
                        }

                        if (member->current_values.supports_white()) {
                            call.set_color_temperature(state->current_values.get_color_temperature());
                        }

                        // Enviar update a HA
                        call.perform();
                    }
                }
            } 
        }
    */

    } // namespace fastcon
} // namespace esphome

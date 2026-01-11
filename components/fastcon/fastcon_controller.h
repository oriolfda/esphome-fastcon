#pragma once

#include <queue>
#include <mutex>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_server/ble_server.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_color_values.h"
#include "esphome/components/light/light_traits.h"

namespace esphome
{
    namespace fastcon
    {

        class FastconController : public Component
        {
        public:
            FastconController() = default;

            void setup() override;
            void loop() override;

            std::vector<uint8_t> get_light_data(light::LightState *state);
            std::vector<uint8_t> single_control(uint32_t addr, 
                                                const std::vector<uint8_t> &light_data,
                                                bool is_group);

            void queueCommand(uint32_t light_id_, const std::vector<uint8_t> &data);

            void clear_queue();
            bool is_queue_empty() const
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                return queue_.empty();
            }
            size_t get_queue_size() const
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                return queue_.size();
            }
            void set_max_queue_size(size_t size) { max_queue_size_ = size; }

            void set_mesh_key(std::array<uint8_t, 4> key) { mesh_key_ = key; }
            void set_adv_interval_min(uint16_t val) { adv_interval_min_ = val; }
            void set_adv_interval_max(uint16_t val)
            {
                adv_interval_max_ = val;
                if (adv_interval_max_ < adv_interval_min_)
                {
                    adv_interval_max_ = adv_interval_min_;
                }
            }
            void set_adv_duration(uint16_t val) { adv_duration_ = val; }
            void set_adv_gap(uint16_t val) { adv_gap_ = val; }

            // Getters per a les variables (opcional)
            bool get_last_has_rgb() const { return last_has_rgb_; }
            bool get_last_has_warm() const { return last_has_warm_; }

            void send_direct_command(uint8_t device_id, bool is_group, bool turn_on, 
                                    float brightness = 1.0f) {
            // 1. Creem els trets de la llum (què pot fer)
            auto traits = light::LightTraits();
            traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
            
            // 2. Creem els valors de color (estat actual)
            auto color_values = light::LightColorValues();
            color_values.set_state(turn_on);
            color_values.set_brightness(brightness);
            color_values.set_color_mode(light::ColorMode::BRIGHTNESS);
            
            // 3. Creem un estat de llum temporal
            // Nota: El constructor pot necessitar un output, li passem nullptr
            auto temp_state = light::LightState(nullptr);
            temp_state.set_traits(traits);
            temp_state.set_current_values(color_values);
            temp_state.set_remote_values(color_values);
            
            // 4. Convertim a light_data (utilitzant el mètode existent)
            auto light_data = get_light_data(&temp_state);
            
            // 5. Enviem per BLE
            single_control(device_id, light_data, is_group);
            
            ESP_LOGI("FASTCON", "Comanda directa: device=%d, group=%d, on=%d, brightness=%.1f",
                    device_id, is_group, turn_on, brightness);
            }

        protected:
            struct Command
            {
                std::vector<uint8_t> data;
                uint32_t timestamp;
                uint8_t retries{0};
                static constexpr uint8_t MAX_RETRIES = 3;
            };

            std::queue<Command> queue_;
            mutable std::mutex queue_mutex_;
            size_t max_queue_size_{100};

            enum class AdvertiseState
            {
                IDLE,
                ADVERTISING,
                GAP
            };

            AdvertiseState adv_state_{AdvertiseState::IDLE};
            uint32_t state_start_time_{0};

            // Protocol implementation
            std::vector<uint8_t> generate_command(uint8_t n, uint32_t light_id_, const std::vector<uint8_t> &data, bool forward = true);

            std::array<uint8_t, 4> mesh_key_{};

            uint16_t adv_interval_min_{0x20};
            uint16_t adv_interval_max_{0x40};
            uint16_t adv_duration_{50};
            uint16_t adv_gap_{10};

            // VARIABLES DE CLASSE AFEGIDES
            bool last_has_rgb_ = false;
            bool last_has_warm_ = false;  // O has_temp, depenent del que vulguis

            static const uint16_t MANUFACTURER_DATA_ID = 0xfff0;
        };

    } // namespace fastcon
} // namespace esphome
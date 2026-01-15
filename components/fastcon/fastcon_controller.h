#pragma once

#include <queue>
#include <mutex>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_server/ble_server.h"
#include <initializer_list>  // Per a set_supported_color_modes
#include "esphome/components/light/light_output.h"  // LightOutput base
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_color_values.h"
#include "esphome/components/light/light_traits.h"
#include "utils.h"


namespace esphome
{
    namespace fastcon
    {

        class DummyLightOutput : public light::LightOutput {
        public:
            DummyLightOutput() = default;
            
            light::LightTraits get_traits() override {
                return traits_;
            }
            
            void write_state(light::LightState* state) override {
                // No-op
            }
            
            void set_traits(const light::LightTraits& traits) {
                traits_ = traits;
            }

        private:
            light::LightTraits traits_;
        };

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

            void register_group(uint8_t group_id, light::LightState* group_light, const std::vector<light::LightState*>& members);

            // Registrar relació grup → membre
            void register_group_member(uint8_t light_id, uint8_t group_id, light::LightState *member);
            // Notificar qualsevol canvi d’estat
            void on_state_changed(
                uint8_t light_id, 
                light::LightState *state);

            // Send direct command from device (touchscreen+ESP32) to lights using BLE.
            void send_direct_command(uint8_t device_id, bool is_group, bool turn_on, 
                                    float brightness = 1.0f,
                                    light::ColorMode color_mode = light::ColorMode::BRIGHTNESS,
                                    const std::string &light_name = "") {
                

                // Simplement defineix-ho així per ara:
                bool has_rgb = false;  // Per a mode 7 (white) sempre false
                bool has_warm = false; // Per a mode 7 (white) sempre false

                // I actualitza:
                last_has_rgb_ = has_rgb;
                last_has_warm_ = has_warm;
                
                char device_id_str[10];
                snprintf(device_id_str, sizeof(device_id_str), "%u", device_id);      

                ESP_LOGI("DEBUG", "========================================");
                ESP_LOGI("DEBUG", "1. Iniciant send_direct_command");
                ESP_LOGI("DEBUG", "   device_id: %u, is_group: %s, turn_on: %s",
                        device_id, is_group ? "true" : "false", turn_on ? "true" : "false");
                ESP_LOGI("DEBUG", "   brightness: %.2f, color_mode: %d",
                        brightness, static_cast<int>(color_mode));
                
                // 1. Traits
                static DummyLightOutput dummy_output;
                static bool traits_initialized = false;
                
                if (!traits_initialized) {
                    ESP_LOGI("DEBUG", "2. Inicialitzant traits per primera vegada");
                    auto traits = light::LightTraits();
                    traits.set_supported_color_modes({
                        light::ColorMode::RGB,
                        light::ColorMode::WHITE,
                        light::ColorMode::BRIGHTNESS,
                        light::ColorMode::COLD_WARM_WHITE
                    });
                    traits.set_min_mireds(153);
                    traits.set_max_mireds(500);
                    dummy_output.set_traits(traits);
                    traits_initialized = true;
                }
                
                // 2. LightState
                ESP_LOGI("DEBUG", "3. Creant LightState");
                auto light_state = light::LightState(&dummy_output);
                
                // 3. Valors de color
                ESP_LOGI("DEBUG", "4. Creant LightColorValues");
                auto color_values = light::LightColorValues();
                color_values.set_state(turn_on);
                color_values.set_brightness(brightness);
                color_values.set_color_mode(color_mode);
                
                ESP_LOGI("DEBUG", "5. Valors configurats - state: %s, brightness: %.2f",
                        turn_on ? "true" : "false", brightness);
                
                // 4. Assigna valors
                ESP_LOGI("DEBUG", "6. Assignant valors a LightState");
                light_state.current_values = color_values;
                light_state.remote_values = color_values;
                light_state.set_name(light_name.c_str());
                light_state.set_object_id(device_id_str);

                // 5. Obtenim dades
                ESP_LOGI("DEBUG", "7. Cridant get_light_data()");
                auto light_data = get_light_data(&light_state);
                
                ESP_LOGI("DEBUG", "8. get_light_data() retorna %u bytes:", 
                        static_cast<unsigned int>(light_data.size()));
                
                // Mostra bytes en hex
                std::string hex_str;
                for (auto byte : light_data) {
                    char buf[4];
                    snprintf(buf, sizeof(buf), "%02X ", byte);
                    hex_str += buf;
                }
                ESP_LOGI("DEBUG", "   Bytes: %s", hex_str.c_str());
                
                // 6. Enviem
                ESP_LOGI("DEBUG", "9. Cridant single_control()");
//                auto result = single_control(device_id, light_data, is_group);
                // Generate the advertisement payload
                auto adv_data2 = single_control(device_id, light_data, is_group);

                // Debug output - print payload as hex
                auto hex_strdata = vector_to_hex_string(adv_data2).data();
                static const char *const TAG = "fastcon.light";
                ESP_LOGD(TAG, "Advertisement Payload (%d bytes): %s", adv_data2.size(), hex_strdata);

                // Send the advertisement
                queueCommand(device_id, adv_data2);

             //   ESP_LOGI("DEBUG", "10. single_control() retorna %u bytes",
             //          static_cast<unsigned int>(result.size()));
                
                // Mostra resultat
            //    std::string result_hex;
            //    for (auto byte : result) {
            //        char buf[4];
            //        snprintf(buf, sizeof(buf), "%02X ", byte);
            //        result_hex += buf;
            //    }
            //    ESP_LOGI("DEBUG", "   Result bytes: %s", result_hex.c_str());
                
                ESP_LOGI("DEBUG", "11. Completat!");
                ESP_LOGI("DEBUG", "========================================");
            }
        
        private:
            struct GroupInfo {
                light::LightState *group;                   // punter al LightState que representa el grup
                std::vector<light::LightState *> members;  // punters a llums que formen part del grup
            };

            std::unordered_map<uint8_t, GroupInfo> groups_;          // group_id -> GroupInfo
            std::unordered_map<uint8_t, std::vector<uint8_t>> light_groups_; // light_id -> grups pare    
   
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
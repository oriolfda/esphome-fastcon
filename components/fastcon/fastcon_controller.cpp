#include "esphome/core/component_iterator.h"
#include "esphome/core/log.h"
#include "esphome/components/light/color_mode.h"
#include "fastcon_controller.h"
#include "protocol.h"

namespace esphome
{
    namespace fastcon
    {
        static const char *const TAG = "fastcon.controller";

        void FastconController::queueCommand(uint32_t light_id_, const std::vector<uint8_t> &data)
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (queue_.size() >= max_queue_size_)
            {
                ESP_LOGW(TAG, "Command queue full (size=%d), dropping command for light %d",
                         queue_.size(), light_id_);
                return;
            }

            Command cmd;
            cmd.data = data;
            cmd.timestamp = millis();
            cmd.retries = 0;

            queue_.push(cmd);
            ESP_LOGV(TAG, "Command queued, queue size: %d", queue_.size());
        }

        void FastconController::clear_queue()
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            std::queue<Command> empty;
            std::swap(queue_, empty);
        }

        void FastconController::setup()
        {
            ESP_LOGCONFIG(TAG, "Setting up Fastcon BLE Controller...");
            ESP_LOGCONFIG(TAG, "  Advertisement interval: %d-%d", this->adv_interval_min_, this->adv_interval_max_);
            ESP_LOGCONFIG(TAG, "  Advertisement duration: %dms", this->adv_duration_);
            ESP_LOGCONFIG(TAG, "  Advertisement gap: %dms", this->adv_gap_);
        }

        void FastconController::loop()
        {
            const uint32_t now = millis();

            switch (adv_state_)
            {
            case AdvertiseState::IDLE:
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (queue_.empty())
                    return;

                Command cmd = queue_.front();
                queue_.pop();

                esp_ble_adv_params_t adv_params = {
                    .adv_int_min = adv_interval_min_,
                    .adv_int_max = adv_interval_max_,
                    .adv_type = ADV_TYPE_NONCONN_IND,
                    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                    .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
                    .channel_map = ADV_CHNL_ALL,
                    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
                };

                uint8_t adv_data_raw[31] = {0};
                uint8_t adv_data_len = 0;

                // Add flags
                adv_data_raw[adv_data_len++] = 2;
                adv_data_raw[adv_data_len++] = ESP_BLE_AD_TYPE_FLAG;
                adv_data_raw[adv_data_len++] = ESP_BLE_ADV_FLAG_BREDR_NOT_SPT | ESP_BLE_ADV_FLAG_GEN_DISC;

                // Add manufacturer data
                adv_data_raw[adv_data_len++] = cmd.data.size() + 2;
                adv_data_raw[adv_data_len++] = ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE;
                adv_data_raw[adv_data_len++] = MANUFACTURER_DATA_ID & 0xFF;
                adv_data_raw[adv_data_len++] = (MANUFACTURER_DATA_ID >> 8) & 0xFF;

                memcpy(&adv_data_raw[adv_data_len], cmd.data.data(), cmd.data.size());
                adv_data_len += cmd.data.size();

                esp_err_t err = esp_ble_gap_config_adv_data_raw(adv_data_raw, adv_data_len);
                if (err != ESP_OK)
                {
                    ESP_LOGW(TAG, "Error setting raw advertisement data (err=%d): %s", err, esp_err_to_name(err));
                    return;
                }

                err = esp_ble_gap_start_advertising(&adv_params);
                if (err != ESP_OK)
                {
                    ESP_LOGW(TAG, "Error starting advertisement (err=%d): %s", err, esp_err_to_name(err));
                    return;
                }

                adv_state_ = AdvertiseState::ADVERTISING;
                state_start_time_ = now;
                ESP_LOGV(TAG, "Started advertising");
                break;
            }

            case AdvertiseState::ADVERTISING:
            {
                if (now - state_start_time_ >= adv_duration_)
                {
                    esp_ble_gap_stop_advertising();
                    adv_state_ = AdvertiseState::GAP;
                    state_start_time_ = now;
                    ESP_LOGV(TAG, "Stopped advertising, entering gap period");
                }
                break;
            }

            case AdvertiseState::GAP:
            {
                if (now - state_start_time_ >= adv_gap_)
                {
                    adv_state_ = AdvertiseState::IDLE;
                    ESP_LOGV(TAG, "Gap period complete");
                }
                break;
            }
            }
        }

        std::vector<uint8_t> FastconController::get_light_data(light::LightState *state)
        {
            std::vector<uint8_t> light_data = {
                0, // 0 - On/Off Bit + 7-bit Brightness
                0, // 1 - Blue byte
                0, // 2 - Red byte
                0, // 3 - Green byte
                0, // 4 - Warm byte
                0  // 5 - Cold byte
            };

            state->dump_config();

            ESP_LOGD("DEBUG_GET_DATA", "get_light_data cridat");
            ESP_LOGI("LIGHTSTATE", "=== LightState rebut ===");
            ESP_LOGI("LIGHTSTATE", "State: %s", state->current_values.is_on() ? "ON" : "OFF");
            ESP_LOGI("LIGHTSTATE", "Brightness: %.2f", state->current_values.get_brightness());
            ESP_LOGI("LIGHTSTATE", "Color mode: %d", state->current_values.get_color_mode());     

            // 1. OBTÉ EL RAW DATA
            uint8_t* raw_ptr = reinterpret_cast<uint8_t*>(&state->current_values);
            size_t raw_size = sizeof(state->current_values);
            
            // 2. MOSTRA EL RAW DATA
            ESP_LOGI("LIGHTSTATE", "=== RAW LightState ===");
            ESP_LOGI("LIGHTSTATE", "Adreça: %p", raw_ptr);
            ESP_LOGI("LIGHTSTATE", "Mida: %zu bytes", raw_size);
            
            // Hex dump complet
            char hex_line[100];
            for (size_t i = 0; i < raw_size; i += 16) {
                memset(hex_line, 0, sizeof(hex_line));
                char *ptr = hex_line;
                ptr += sprintf(ptr, "[%04zx]: ", i);
                
                for (size_t j = 0; j < 16 && (i + j) < raw_size; j++) {
                    ptr += sprintf(ptr, "%02X ", raw_ptr[i + j]);
                }
                ESP_LOGI("LIGHTSTATE", "%s", hex_line);
            }


            // TODO: need to figure out when esphome is changing to white vs setting brightness
            
            auto values = state->current_values;
            
            bool is_on = values.is_on();
            if (!is_on)
            {
                return std::vector<uint8_t>({0x00});
            }

            auto color_mode = values.get_color_mode();
            bool has_white = (static_cast<uint8_t>(color_mode) & static_cast<uint8_t>(light::ColorCapability::WHITE)) != 0;
            float brightness = std::min(values.get_brightness() * 127.0f, 127.0f); // clamp the value to at most 127
            light_data[0] = 0x80 + static_cast<uint8_t>(brightness);

            if (has_white)
            {
                return std::vector<uint8_t>({static_cast<uint8_t>(brightness)});
                // DEBUG: when changing to white mode, this should be the payload:
                // ff0000007f7f
            }

            bool has_rgb = (static_cast<uint8_t>(color_mode) & static_cast<uint8_t>(light::ColorCapability::RGB)) != 0;
            last_has_rgb_ = has_rgb;
    
            if (has_rgb)
            {
                light_data[1] = static_cast<uint8_t>(values.get_blue() * 255.0f);
                light_data[2] = static_cast<uint8_t>(values.get_red() * 255.0f);
                light_data[3] = static_cast<uint8_t>(values.get_green() * 255.0f);
            }

            bool has_cold_warm = (static_cast<uint8_t>(color_mode) & static_cast<uint8_t>(light::ColorCapability::COLD_WARM_WHITE)) != 0;
            last_has_warm_ = has_cold_warm;
            if (has_cold_warm)
            {
                light_data[4] = static_cast<uint8_t>(values.get_warm_white() * 255.0f);
                light_data[5] = static_cast<uint8_t>(values.get_cold_white() * 255.0f);
            }

            // TODO figure out if we can use these, and how
            bool has_temp = (static_cast<uint8_t>(color_mode) & static_cast<uint8_t>(light::ColorCapability::COLOR_TEMPERATURE)) != 0;
            if (has_temp)
            {
                float temperature = values.get_color_temperature();
                if (temperature < 153)
                {
                    light_data[4] = 0xff;
                    light_data[5] = 0x00;
                }
                else if (temperature > 500)
                {
                    light_data[4] = 0x00;
                    light_data[5] = 0xff;
                }
                else
                {
                    // Linear interpolation between (153, 0xff) and (500, 0x00)
                    light_data[4] = (uint8_t)(((500 - temperature) * 255.0f + (temperature - 153) * 0x00) / (500 - 153));
                    light_data[5] = (uint8_t)(((temperature - 153) * 255.0f + (500 - temperature) * 0x00) / (500 - 153));
                }
            }
            ESP_LOGD("DEBUG_GET_DATA", "get_light_data retorna %u bytes", 
                    static_cast<unsigned int>(light_data.size()));
            return light_data;
        }

        std::vector<uint8_t> FastconController::single_control(
            uint32_t light_id_, 
                const std::vector<uint8_t> &light_data,
                bool is_group
            ) {  // <-- NEW PARAMETER

            std::vector<uint8_t> result_data(12);
            
            if (is_group) {
                // FORMAT GROUP: 43 2A A8 [ID_GROUP has been moved to light_id in fastcon_light.h]
                if (last_has_rgb_ | last_has_warm_)
                {
                    result_data[0] = 0x93;
                }
                else{
                    result_data[0] = 0x43;
                }
                result_data[1] = 0x2A;
                result_data[2] = 0xA8;
                result_data[3] = light_id_ & 0xFF;
                std::copy(light_data.begin(), light_data.end(), result_data.begin() + 4);
                
                ESP_LOGD(TAG, "Generant comanda GRUP id=%d", light_id_);
                ESP_LOGD(TAG, "Result data=%d", result_data);
            } else {
                // FORMAT SINGLE LIGHT
                result_data[0] = 2 | (((0x0FFFFFF & (light_data.size() + 1)) << 4));
                result_data[1] = light_id_;
                std::copy(light_data.begin(), light_data.end(), result_data.begin() + 2);
                
                ESP_LOGD(TAG, "Generant comanda INDIVIDUAL id=%d", light_id_);
            }

            // Debug output - print payload as hex
            auto hex_str = vector_to_hex_string(result_data).data();
            ESP_LOGD(TAG, "Inner Payload (%d bytes): %s", result_data.size(), hex_str);

            return this->generate_command(5, light_id_, result_data, true);
        }

        std::vector<uint8_t> FastconController::generate_command(uint8_t n, uint32_t light_id_, const std::vector<uint8_t> &data, bool forward)
        {
            static uint8_t sequence = 0;

            // Create command body with header
            std::vector<uint8_t> body(data.size() + 4);
            uint8_t i2 = (light_id_ / 256);

            // Construct header
            body[0] = (i2 & 0b1111) | ((n & 0b111) << 4) | (forward ? 0x80 : 0);
            body[1] = sequence++; // Use and increment sequence number
            if (sequence >= 255)
                sequence = 1;

            body[2] = this->mesh_key_[3]; // Safe key

            // Copy data
            std::copy(data.begin(), data.end(), body.begin() + 4);

            // Calculate checksum
            uint8_t checksum = 0;
            for (size_t i = 0; i < body.size(); i++)
            {
                if (i != 3)
                {
                    checksum = checksum + body[i];
                }
            }
            body[3] = checksum;

            // Encrypt header and data
            for (size_t i = 0; i < 4; i++)
            {
                body[i] = DEFAULT_ENCRYPT_KEY[i & 3] ^ body[i];
            }

            for (size_t i = 0; i < data.size(); i++)
            {
                body[4 + i] = this->mesh_key_[i & 3] ^ body[4 + i];
            }

            // Prepare the final payload with RF protocol formatting
            std::vector<uint8_t> addr = {DEFAULT_BLE_FASTCON_ADDRESS.begin(), DEFAULT_BLE_FASTCON_ADDRESS.end()};
            return prepare_payload(addr, body);
        }
       
        void FastconController::dump_groups() {
            ESP_LOGD(TAG, "========== FASTCON GROUP MAP DUMP ==========");

            ESP_LOGD(TAG, "groups_ size: %d", groups_.size());
            for (auto &g : groups_) {
                uint8_t group_id = g.first;
                auto &info = g.second;

                ESP_LOGD(TAG, "Group ID %d:", group_id);
                ESP_LOGD(TAG, "  group LightState ptr: %p", info.group);
                ESP_LOGD(TAG, "  members count: %d", info.members.size());

                int idx = 0;
                for (auto *m : info.members) {
                    ESP_LOGD(TAG, "    [%d] member LightState ptr: %p", idx++, m);
                }
            }

            ESP_LOGD(TAG, "light_groups_ size: %d", light_groups_.size());
            for (auto &lg : light_groups_) {
                uint8_t light_id = lg.first;
                auto &groups = lg.second;

                ESP_LOGD(TAG, "Light ID %d belongs to %d group(s):", light_id, groups.size());
                for (auto gid : groups) {
                    ESP_LOGD(TAG, "    -> group ID %d", gid);
                }
            }

            ESP_LOGD(TAG, "============================================");
        }


        // Registrar un grup amb IDs i punters als members
        void FastconController::register_group(
            uint8_t group_id,
            uint8_t group_light_id,
            light::LightState* group_light,
            const std::vector<std::pair<uint8_t, light::LightState*>>& members) {

            if (!group_light) return;

            GroupInfo info;
            info.group = group_light;
            info.members.clear();

            for (auto &p : members) {
                uint8_t light_id = p.first;
                light::LightState* member = p.second;
                if (!member) continue;

                // Afegir member al vector del grup
                info.members.push_back(member);

                // Registrar el grup al light_id del member
                light_groups_[light_id].push_back(group_id);
            }

            // Registrar el grup complet
            groups_[group_id] = info;

            ESP_LOGD(TAG, "Registered group ID %d with %d members", group_id, info.members.size());
            dump_groups();
        }

        // Registrar un member individual (encara disponible si cal)
        void FastconController::register_group_member(uint8_t light_id, uint8_t group_id, light::LightState* member) {
            if (!member) return;

            auto &members = groups_[group_id].members;
            if (std::find(members.begin(), members.end(), member) == members.end()) {
                members.push_back(member);
            }

            auto &lg_vec = light_groups_[light_id];
            if (std::find(lg_vec.begin(), lg_vec.end(), group_id) == lg_vec.end()) {
                lg_vec.push_back(group_id);
            }

            ESP_LOGD(TAG, "Registered light ID %d to group ID %d", light_id, group_id);
        }


        void FastconController::on_state_changed(uint8_t light_id, light::LightState *state) {
            if (!state)
                return;

            // 1️⃣ Propagar estat als grups als quals pertany el llum individual
            auto git = light_groups_.find(light_id);
            if (git != light_groups_.end()) {
                for (auto group_id : git->second) {
                    auto &group_info = groups_[group_id];

                    bool all_on = true;
                    for (auto* m : group_info.members) {
                        if (!m->current_values.is_on()) {
                            all_on = false;
                            break;
                        }
                    }

                    auto* group_light = group_info.group;
                    if (group_light && group_light->current_values.is_on() != all_on) {
                        auto call = group_light->make_call();
                        call.set_state(all_on);
                        call.perform();
                    }
                }
            }

            // 2️⃣ Si el canvi és sobre un grup, propagar als membres
            auto git2 = groups_.find(light_id);
            if (git2 != groups_.end()) {
                auto &members = git2->second.members;
                auto* group_light = git2->second.group;
                bool is_on = group_light->current_values.is_on();

                for (auto* member : members) {
                    if (!member)
                        continue;

                    auto call = member->make_call();
                    call.set_state(is_on);
                    call.perform();
                }
            }
        }

            
        }
    } // namespace fastcon
} // namespace esphome

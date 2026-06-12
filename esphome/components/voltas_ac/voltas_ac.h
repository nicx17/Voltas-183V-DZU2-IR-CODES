#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/core/component.h"

namespace esphome {
namespace voltas_ac {

// Voltas 183V DZU2 IR Protocol
// 48-bit NEC-extended variant, 2 identical frames, 38kHz carrier
//
// Byte layout: B0 B1 B2 B3 B4 B5
//   B0/B1 = device address
//   B2    = command (fan speed / timer encoding)
//   B3    = ~B2
//   B4    = data (temp nibble << 4 | mode/page nibble)
//   B5    = ~B4 (except timer commands)

// Standard device address
static const uint8_t VOLTAS_ADDR_HI = 0xB2;
static const uint8_t VOLTAS_ADDR_LO = 0x4D;

// Special device address (turbo, display toggle)
static const uint8_t SPECIAL_ADDR_HI = 0xB5;
static const uint8_t SPECIAL_ADDR_LO = 0x4A;
static const uint8_t SPECIAL_CMD = 0xF5;

// IR timing (microseconds)
static const uint16_t HEADER_MARK = 4450;
static const uint16_t HEADER_SPACE = 4450;
static const uint16_t BIT_MARK = 530;
static const uint16_t ONE_SPACE = 1650;
static const uint16_t ZERO_SPACE = 530;
static const uint16_t FRAME_GAP = 5200;

// Fan speed -> B2 byte (bits 7-6 encode speed)
static const uint8_t FAN_HIGH = 0x3F;
static const uint8_t FAN_MEDIUM = 0x5F;
static const uint8_t FAN_LOW = 0x9F;
static const uint8_t FAN_AUTO = 0xBF;

// Mode -> lower nibble of B4
static const uint8_t MODE_COOL = 0x00;
static const uint8_t MODE_DRY = 0x04;
static const uint8_t MODE_HEAT = 0x0C;
static const uint8_t MODE_FAN = 0x04;

// Special B2 values (standard address)
static const uint8_t CMD_POWER_OFF = 0x7B;
static const uint8_t CMD_SWING_ON = 0x6B;
static const uint8_t CMD_SWING_OFF = 0x0F;
static const uint8_t CMD_DRY_MODE = 0x1F;

// Special data bytes (special address B5 4A)
static const uint8_t DATA_TURBO = 0xA2;
static const uint8_t DATA_DISPLAY_OFF = 0xA5;
static const uint8_t DISPLAY_OFF_B5 = 0x5A;
static const uint8_t DATA_DISPLAY_TOGGLE = 0xA5;
static const uint8_t DISPLAY_TOGGLE_B5 = 0x2D;

// Sleep mode (standard address)
static const uint8_t CMD_SLEEP_ON = 0xE0;
static const uint8_t SLEEP_ON_B4 = 0x03;
static const uint8_t CMD_SLEEP_OFF = 0xAB; // also used for follow_me off

// Eco mode (special address B5 4A)
static const uint8_t ECO_ON_B4 = 0x82;
static const uint8_t ECO_OFF_B4 = 0x83;
static const uint8_t ECO_B5 = 0x90; // non-standard B5

// Follow me (unique address BA 45)
static const uint8_t FOLLOW_ADDR_HI = 0xBA;
static const uint8_t FOLLOW_ADDR_LO = 0x45;

// Temperature (Celsius) -> Gray-coded upper nibble of B4
static uint8_t temp_to_nibble(uint8_t temp) {
  static const uint8_t TABLE[] = {
      //   17C  18C  19C  20C
      0x0,
      0x1,
      0x3,
      0x2,
      //   21C  22C  23C  24C
      0x6,
      0x7,
      0x5,
      0x4,
      //   25C  26C  27C  28C
      0xC,
      0xD,
      0x9,
      0x8,
      //   29C  30C
      0xA,
      0xB,
  };
  if (temp < 17)
    temp = 17;
  if (temp > 30)
    temp = 30;
  return TABLE[temp - 17];
}

// Timer OFF encoding:
//   half_hours = hours * 2 (1-48, representing 0.5h to 24h)
//   page  = (half_hours - 1) / 16   (0, 1, or 2)
//   index = (half_hours - 1) % 16   (0-15)
//   B2 = 0x21 + index * 2
//   B4 = (temp_nibble << 4) | page
//   B5 = 0xFF  (NOT the standard ~B4)
//
// Timer ON encoding:
//   B2 = normal fan byte
//   B4 = (temp_nibble << 4) | 0x03
//   B5 = 0x81 + (half_hours - 1) * 2  (NOT the standard ~B4)

class VoltasAC : public climate::Climate, public Component {
public:
  void
  set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }

  void setup() override {
    auto restore = this->restore_state_();
    if (restore.has_value()) {
      restore->apply(this);
    } else {
      this->mode = climate::CLIMATE_MODE_OFF;
      this->target_temperature = 24;
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
  }

  float get_setup_priority() const override { return setup_priority::DATA; }

  climate::ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();

    traits.set_visual_min_temperature(17);
    traits.set_visual_max_temperature(30);
    traits.set_visual_temperature_step(1);

    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_FAN_ONLY,
    });

    traits.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
    });

    traits.set_supported_swing_modes({
        climate::CLIMATE_SWING_OFF,
        climate::CLIMATE_SWING_VERTICAL,
    });

    return traits;
  }

  void control(const climate::ClimateCall &call) override {
    if (call.get_mode().has_value())
      this->mode = *call.get_mode();
    if (call.get_target_temperature().has_value())
      this->target_temperature = *call.get_target_temperature();
    if (call.get_fan_mode().has_value())
      this->fan_mode = *call.get_fan_mode();

    // Handle swing mode changes with dedicated commands
    if (call.get_swing_mode().has_value()) {
      auto sw = *call.get_swing_mode();
      this->swing_mode = sw;
      if (sw == climate::CLIMATE_SWING_VERTICAL) {
        this->send_frame_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, CMD_SWING_ON, 0xE0);
      } else {
        this->send_frame_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, CMD_SWING_OFF, 0xE0);
      }
      this->publish_state();
      return;
    }

    this->transmit_state_();
    this->publish_state();
  }

  // --- Public methods for HA buttons/switches/numbers ---

  void send_turbo_toggle() {
    // Turbo toggle: B5 4A F5 0A A2 5D
    this->send_frame_(SPECIAL_ADDR_HI, SPECIAL_ADDR_LO, SPECIAL_CMD,
                      DATA_TURBO);
    ESP_LOGD("voltas_ac", "Turbo toggled");
  }

  void send_display_toggle() {
    // Display toggle: B5 4A F5 0A A5 2D
    this->send_frame_raw_(SPECIAL_ADDR_HI, SPECIAL_ADDR_LO, SPECIAL_CMD,
                          DATA_DISPLAY_TOGGLE, DISPLAY_TOGGLE_B5);
    ESP_LOGD("voltas_ac", "Display toggled");
  }

  void send_sleep_on() {
    // Sleep ON: B2 4D E0 1F 03 FC
    // Fan goes to auto, AC gradually adjusts temp overnight
    this->send_frame_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, CMD_SLEEP_ON,
                      SLEEP_ON_B4);
    ESP_LOGD("voltas_ac", "Sleep ON");
  }

  void send_sleep_off() {
    // Sleep OFF: B2 4D AB 54 <temp_byte> FF
    // Returns to normal operation
    uint8_t temp = (uint8_t)this->target_temperature;
    uint8_t temp_nib = temp_to_nibble(temp);
    uint8_t b4 = (temp_nib << 4) | 0x01;
    this->send_frame_raw_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, CMD_SLEEP_OFF, b4,
                          0xFF);
    ESP_LOGD("voltas_ac", "Sleep OFF");
  }

  void send_eco_on() {
    // Eco ON: B5 4A F5 0A 82 90
    this->send_frame_raw_(SPECIAL_ADDR_HI, SPECIAL_ADDR_LO, SPECIAL_CMD,
                          ECO_ON_B4, ECO_B5);
    ESP_LOGD("voltas_ac", "Eco ON");
  }

  void send_eco_off() {
    // Eco OFF: B5 4A F5 0A 83 90
    this->send_frame_raw_(SPECIAL_ADDR_HI, SPECIAL_ADDR_LO, SPECIAL_CMD,
                          ECO_OFF_B4, ECO_B5);
    ESP_LOGD("voltas_ac", "Eco OFF");
  }

  void send_follow_me_on() {
    // Follow me ON: BA 45 5F A0 92 6D
    // Uses temp sensor in remote as reference
    uint8_t temp = (uint8_t)this->target_temperature;
    uint8_t temp_nib = temp_to_nibble(temp);
    uint8_t b4 = (temp_nib << 4) | 0x02;
    this->send_frame_(FOLLOW_ADDR_HI, FOLLOW_ADDR_LO, FAN_MEDIUM, b4);
    ESP_LOGD("voltas_ac", "Follow Me ON");
  }

  void send_follow_me_off() {
    // Follow me OFF = same as sleep off: B2 4D AB 54 <temp> FF
    uint8_t temp = (uint8_t)this->target_temperature;
    uint8_t temp_nib = temp_to_nibble(temp);
    uint8_t b4 = (temp_nib << 4) | 0x01;
    this->send_frame_raw_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, CMD_SLEEP_OFF, b4,
                          0xFF);
    ESP_LOGD("voltas_ac", "Follow Me OFF");
  }

  void send_timer_off(float hours) {
    // Schedule AC to turn OFF after 'hours'
    //   0.5h to 10h: 0.5h steps allowed
    //   11h to 24h:  whole hours only
    //   hours=0: cancel timer
    if (hours <= 0) {
      this->transmit_state_();
      ESP_LOGD("voltas_ac", "Timer OFF cancelled");
      return;
    }

    // Snap to valid values: above 10h only whole hours exist
    if (hours > 10.0f) {
      hours = (float)(int)(hours + 0.5f); // round to nearest whole hour
      if (hours < 11.0f)
        hours = 11.0f;
    }
    if (hours > 24.0f)
      hours = 24.0f;

    int half_hours = (int)(hours * 2);
    if (half_hours < 1)
      half_hours = 1;
    if (half_hours > 48)
      half_hours = 48;

    int page = (half_hours - 1) / 16;
    int index = (half_hours - 1) % 16;

    uint8_t b2 = 0x21 + index * 2;
    uint8_t temp = (uint8_t)this->target_temperature;
    uint8_t temp_nib = temp_to_nibble(temp);
    uint8_t b4 = (temp_nib << 4) | page;

    this->send_frame_raw_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, b2, b4, 0xFF);
    ESP_LOGD("voltas_ac", "Timer OFF set: %.1f hours (half_hours=%d)", hours,
             half_hours);
  }

  void send_timer_on(float hours) {
    // Schedule AC to turn ON after 'hours' (0.5 to 24, in 0.5h steps)
    if (hours <= 0) {
      this->transmit_state_();
      ESP_LOGD("voltas_ac", "Timer ON cancelled");
      return;
    }

    int half_hours = (int)(hours * 2);
    if (half_hours < 1)
      half_hours = 1;
    if (half_hours > 48)
      half_hours = 48;

    uint8_t b2 = this->get_fan_byte_();
    uint8_t temp = (uint8_t)this->target_temperature;
    uint8_t temp_nib = temp_to_nibble(temp);
    uint8_t b4 = (temp_nib << 4) | 0x03;
    uint8_t b5 = 0x81 + (half_hours - 1) * 2;

    this->send_frame_raw_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, b2, b4, b5);
    ESP_LOGD("voltas_ac", "Timer ON set: %.1f hours", hours);
  }

protected:
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};

  uint8_t get_fan_byte_() {
    switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_HIGH:
      return FAN_HIGH;
    case climate::CLIMATE_FAN_MEDIUM:
      return FAN_MEDIUM;
    case climate::CLIMATE_FAN_LOW:
      return FAN_LOW;
    default:
      return FAN_AUTO;
    }
  }

  void transmit_state_() {
    // Handle OFF
    if (this->mode == climate::CLIMATE_MODE_OFF) {
      this->send_frame_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, CMD_POWER_OFF, 0xE0);
      return;
    }

    uint8_t b2 = this->get_fan_byte_();

    // Temperature -> upper nibble of B4
    uint8_t temp = (uint8_t)this->target_temperature;
    uint8_t temp_nib = temp_to_nibble(temp);

    // Mode -> lower nibble of B4
    uint8_t mode_nib;
    switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      mode_nib = MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      mode_nib = MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_DRY:
      mode_nib = MODE_DRY;
      b2 = CMD_DRY_MODE;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      mode_nib = MODE_FAN;
      temp_nib = 0xE;
      break;
    default:
      mode_nib = MODE_COOL;
      break;
    }

    uint8_t b4 = (temp_nib << 4) | mode_nib;
    this->send_frame_(VOLTAS_ADDR_HI, VOLTAS_ADDR_LO, b2, b4);
  }

  // Send frame with B5 = ~B4 (standard checksum)
  void send_frame_(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b4) {
    this->send_frame_raw_(b0, b1, b2, b4, ~b4);
  }

  // Send frame with explicit B5 (for timer/display commands)
  void send_frame_raw_(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b4,
                       uint8_t b5) {
    uint8_t b3 = ~b2;
    uint8_t bytes[6] = {b0, b1, b2, b3, b4, b5};

    auto transmit = this->transmitter_->transmit();
    auto *data = transmit.get_data();

    data->set_carrier_frequency(38000);

    // Send two identical frames
    for (int frame = 0; frame < 2; frame++) {
      data->mark(HEADER_MARK);
      data->space(HEADER_SPACE);

      for (int byte_idx = 0; byte_idx < 6; byte_idx++) {
        for (int bit = 7; bit >= 0; bit--) {
          data->mark(BIT_MARK);
          if (bytes[byte_idx] & (1 << bit)) {
            data->space(ONE_SPACE);
          } else {
            data->space(ZERO_SPACE);
          }
        }
      }

      data->mark(BIT_MARK);

      if (frame == 0) {
        data->space(FRAME_GAP);
      }
    }

    transmit.perform();

    ESP_LOGD("voltas_ac", "Sent: %02X %02X %02X %02X %02X %02X", bytes[0],
             bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
  }
};

} // namespace voltas_ac
} // namespace esphome

#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::st77922_touch {

static const char *const TAG = "st77922_touch";

// 16-bit register addresses, big-endian on the wire.
static const uint16_t TREG_FW_VERSION = 0x0000;  // 1 byte
static const uint16_t TREG_MAX_X_H = 0x0005;     // maxX hi/lo, maxY hi/lo, max points
static const uint16_t TREG_TOUCH_INFO = 0x0010;  // bit 3 set = a report is waiting
static const uint16_t TREG_REPORT_0 = 0x0014;    // 7 bytes per contact

static const uint8_t TOUCH_INFO_WITH_COORD = 0x08;
static const uint8_t REPORT_STRIDE = 7;
static const uint8_t MAX_REPORTED_POINTS = 10;

class ST77922Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

 protected:
  bool read_reg16_(uint16_t reg, uint8_t *data, size_t len);

  InternalGPIOPin *interrupt_pin_{nullptr};
  uint8_t max_points_{5};
  uint8_t fw_version_{0};
};

}  // namespace esphome::st77922_touch

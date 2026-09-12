#include "st77922_touchscreen.h"

#include "esphome/core/helpers.h"

namespace esphome::st77922_touch {

bool ST77922Touchscreen::read_reg16_(uint16_t reg, uint8_t *data, size_t len) {
  return this->read_register16(reg, data, len) == i2c::ERROR_OK;
}

void ST77922Touchscreen::setup() {
  // The reset line is shared with the LCD on this board, so it must be pulsed
  // before the display is initialised — the YAML does it from on_boot rather
  // than here, where it would wipe the panel init.
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  uint8_t info[5] = {};
  if (!this->read_reg16_(TREG_FW_VERSION, &this->fw_version_, 1) ||
      !this->read_reg16_(TREG_MAX_X_H, info, sizeof(info))) {
    ESP_LOGE(TAG, "Controller did not answer at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // Reports arrive in the controller's own units. If those are not 1:1 with
  // the panel, every touch lands silently offset, so feed the reported
  // geometry into the base class calibration instead of only logging it. A
  // zero or absurd value means the read was garbage — fall back to the
  // display's own size rather than scaling by it.
  const uint16_t native_w = (info[0] << 8) | info[1];
  const uint16_t native_h = (info[2] << 8) | info[3];

  if (this->x_raw_max_ == this->x_raw_min_) {
    this->x_raw_max_ = (native_w > 0 && native_w <= 4096) ? native_w : this->display_->get_native_width();
  }
  if (this->y_raw_max_ == this->y_raw_min_) {
    this->y_raw_max_ = (native_h > 0 && native_h <= 4096) ? native_h : this->display_->get_native_height();
  }

  if (info[4] >= 1 && info[4] <= MAX_REPORTED_POINTS) {
    this->max_points_ = info[4];
  }
}

void ST77922Touchscreen::update_touches() {
  uint8_t info = 0;
  if (!this->read_reg16_(TREG_TOUCH_INFO, &info, 1)) {
    this->status_set_warning();
    return;
  }

  // No report pending says nothing about whether a finger is down, so skip the
  // update entirely rather than reporting an empty (and therefore releasing)
  // set of touches.
  if ((info & TOUCH_INFO_WITH_COORD) == 0) {
    this->skip_update_ = true;
    return;
  }

  uint8_t report[REPORT_STRIDE * MAX_REPORTED_POINTS];
  const size_t len = REPORT_STRIDE * this->max_points_;
  if (!this->read_reg16_(TREG_REPORT_0, report, len)) {
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  // Per contact: [0] valid(bit7) + xH(bits 0..5), [1] xL, [2] yH, [3] yL,
  // [4] area, [5] intensity, [6] reserved.
  for (uint8_t i = 0; i < this->max_points_; i++) {
    const uint8_t *r = report + REPORT_STRIDE * i;
    if ((r[0] & 0x80) == 0) {
      continue;
    }
    const int16_t x = ((r[0] & 0x3F) << 8) | r[1];
    const int16_t y = (r[2] << 8) | r[3];
    this->add_raw_touch_position_(i, x, y, r[4]);
  }
}

void ST77922Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "ST77922 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  ESP_LOGCONFIG(TAG,
                "  Firmware version: %u\n"
                "  Max touch points: %u\n"
                "  Raw range: %d x %d",
                this->fw_version_, this->max_points_, this->x_raw_max_, this->y_raw_max_);
}

}  // namespace esphome::st77922_touch

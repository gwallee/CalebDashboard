"""ESPHome driver for the touch block built into the ST77922 panel controller.

The Elecrow DLE06235B exposes its touch controller on a plain I2C bus at 0x55
using 16-bit register addresses. It answers none of the usual probes (it is not
an FT5x06, GT911 or CST816), so it needs its own driver. The register map and
report layout here were taken from the working Arduino sketch for this board,
which in turn followed the vendor's ESP_LCD_TOUCH_IO_I2C_ST77922_CONFIG().
"""

import esphome.codegen as cg

DEPENDENCIES = ["i2c"]

st77922_touch_ns = cg.esphome_ns.namespace("st77922_touch")

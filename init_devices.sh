#!/bin/bash

# Load Kernel Modules
insmod fpga_push_switch_driver.ko
insmod fpga_dot_driver.ko
insmod fpga_fnd_driver.ko
insmod fpga_text_lcd_driver.ko
insmod fpga_buzzer_driver.ko

# Create device nodes (replace MAJOR numbers if needed)
mknod /dev/fpga_push_switch c 265 0
mknod /dev/fpga_dot c 262 0
mknod /dev/fpga_fnd c 261 0
mknod /dev/fpga_text_lcd c 263 0
mknod /dev/fpga_buzzer c 264 0

# Set permissions
chmod 666 /dev/fpga_*
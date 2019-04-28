#!/bin/bash
sudo insmod secondfs.ko
sudo echo ====== > /dev/kmsg
sudo rmmod secondfs.ko
dmesg | tail

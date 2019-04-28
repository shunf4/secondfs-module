#!/bin/bash
sudo insmod secondfs.ko
echo ====== | sudo dd of=/dev/kmsg > /dev/null 2>&1
sudo rmmod secondfs.ko
dmesg | tail

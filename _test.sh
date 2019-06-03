#!/bin/bash
sudo insmod secondfs.ko
mkdir -p test test2
sudo mount -t secondfs -o loop new.img ./test2
sudo mount -t secondfs -o loop c.img ./test

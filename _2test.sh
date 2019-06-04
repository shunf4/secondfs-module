#!/bin/bash
sudo insmod secondfs.ko
sudo umount test2
mkdir -p test test2 test3
dd if=/dev/urandom of=new.img bs=512 count=2048
./mkfs.secondfs new.img

sudo mount -t secondfs -o loop new.img ./test2

cd test2
mkdir abc
mkdir abc/def abc/ghi abc/jkl
mkdir abc/def/123
touch abc/def/456.txt

ls -R
stat . abc abc/def abc/ghi abc/def/123 abc/def/456.txt

rm -rf ../test3/*
cp -R * ../test3/

cd ../test3

ls -R
stat . abc abc/def abc/ghi abc/def/123 abc/def/456.txt

cd ../test2

ls -R
stat . abc abc/def abc/ghi abc/def/123 abc/def/456.txt
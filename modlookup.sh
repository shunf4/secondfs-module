#!/bin/bash
echo "file secondfs.ko" > ./gdb.tmp.script
echo "list *$1" >> ./gdb.tmp.script
gdb --command=./gdb.tmp.script


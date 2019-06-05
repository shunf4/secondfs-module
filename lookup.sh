#!/bin/bash
cp ./gdb.tmp.script ./gdb.tmp.script
echo "list *$1" >> ./gdb.tmp.script
gdb --command=./gdb.tmp.script


#!/bin/bash

make ./daemon
./daemon start
ps -axj | grep ./daemon
sleep 10
ls -sh ~/random/buffer
truncate ~/random/buffer --size 1M
ls -sh ~/random/buffer
sleep 10
./daemon stop
ps -axj | grep ./daemon
ls -sh ~/random/buffer
rngtest < ~/random/buffer
rm ~/random/buffer

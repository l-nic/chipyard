#!/usr/bin/env bash

sudo ip tuntap add mode tap dev tap0 user $USER
sudo ip link set tap0 up
sudo ip addr add 192.168.1.1/24 dev tap0
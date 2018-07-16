#!/bin/bash

iptables --table nat --append POSTROUTING --out-interface eth0 -j MASQUERADE
iptables --append FORWARD --in-interface eth1 -j ACCEPT
iptables --table nat --list
sh -c "echo 1 > /proc/sys/net/ipv4/ip_forward"
#!/bin/bash

exportfs -rav
/etc/init.d/nfs-kernel-server restart
showmount -e

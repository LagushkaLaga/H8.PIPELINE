#!bin/bash

find / "hailo_pci.ko" -exec insmod {} \;

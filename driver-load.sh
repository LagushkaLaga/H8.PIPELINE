#!bin/bash

sudo find / "hailo_pci.ko" -exec insmod {} \;

#!bin/bash

find / "pcie_hailo.ko" -exec insmod {} \;

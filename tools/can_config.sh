sudo ip link set can0 up type can bitrate 1000000 dbitrate 2000000 restart-ms 1000 berr-reporting on fd on
sudo ifconfig can0 txqueuelen 65536
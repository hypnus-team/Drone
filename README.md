Drone
=====

client of remote control system

<How>

compile Drone
1.cd ./drone
2.make


compile bootloader
1.cd ./bootloader
2.vi bootloader.c
3.modify static char droneConf[160] 's value£¬TokenID,Port,Domain,Host,Web Path
4.gcc bootloader -ldl

combine Drone and bootloader,then run
1.cat ./drone.so >> ./bootloader
2.chmod +x ./bootloader
3../bootloader

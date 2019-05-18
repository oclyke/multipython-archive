# PORT = /dev/cu.usbserial-1410
# PORT = /dev/cu.usbserial-1420
# PORT = /dev/cu.usbserial-00002014B
# PORT = /dev/cu.usbserial-00001014B
PORT = /dev/cu.wchusbserial1410

# SDKCONFIG = boards/sdkconfig
SDKCONFIG = boards/sdkconfig.spiram_malloc_caps

BAUD = 2000000
FLASH_MODE = dio
FLASH_FREQ = 40m
FLASH_SIZE = 4MB

ESPIDF=/Users/owenlyke/EchoicTech/Products/reverb/esp-idf
# ESPIDF=/Users/owenlyke/esp/esp-idf
PYTHON2=python

# # partitions ( use for OTA - select your custom partition table )
# PART_SRC = partitions.csv
# PART_SRC = partitions_ota.csv
# PART_SRC = partitions_large_app.csv
PART_SRC =partitions_ota_16MB.csv

include Makefile

flash: deploy
# PORT = /dev/cu.usbserial-1410
PORT = /dev/cu.usbserial-1420
# PORT = /dev/cu.usbserial-00002014B

SDKCONFIG = boards/sdkconfig
# SDKCONFIG = boards/sdkconfig.spiram_malloc_caps

BAUD = 2000000
FLASH_MODE = dio
FLASH_FREQ = 40m
FLASH_SIZE = 4MB

ESPIDF=/Users/owenlyke/EchoicTech/Products/reverb/esp-idf
PYTHON2=python

include Makefile

flash: deploy
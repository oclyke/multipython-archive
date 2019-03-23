REVERBDIR=/Users/owenlyke/EchoicTech/Products/reverb

# # Overriding variables from micropython build process
ESPIDF=$(REVERBDIR)/esp-idf
PYTHON2=python

# ESPIDF = /Users/owenlyke/Desktop/Temporary/esp-idf# Temporary

PORT = /dev/cu.usbserial-1410
BAUD = 2000000
FLASH_MODE = dio
FLASH_FREQ = 40m
FLASH_SIZE = 4MB

include Makefile

flash: deploy
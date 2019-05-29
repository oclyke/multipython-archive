import gc
import uos
import mach1
from flashbdev import bdev

try:
    if bdev:
        uos.mount(bdev, '/')
except OSError:
    import inisetup
    vfs = inisetup.setup()

from sdbdev import sdbdevice

try:
    if sdbdevice:
        uos.mount(sdbdevice, '/sdcard')
except OSError:
    print('No SD Block Device')

mach1._boot()

gc.collect()

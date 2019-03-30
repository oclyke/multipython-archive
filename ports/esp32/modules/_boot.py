import gc
import uos
from flashbdev import bdev
from sdbdev import sdbdev

try:
    if bdev:
        uos.mount(bdev, '/')
except OSError:
    import inisetup
    vfs = inisetup.setup()

try:
    if sdbdev:
        uos.mount(sdbdev, '/sdcard')
except OSError:
    print('Could not initialize SD card')

gc.collect()

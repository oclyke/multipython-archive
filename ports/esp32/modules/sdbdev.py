import sdmmc

class SDBdev:

    # SEC_SIZE = 4096 
    # START_SEC = esp.flash_user_start() // SEC_SIZE

    def __init__(self):
        # # put code here like starting the io functions
        # print('SDBdev init called')
        status = sdmmc.init()
        if(status == True):
            self.blocks = sdmmc.get_num_blocks()
            self.sec_size = sdmmc.get_block_size()
        else:
            # print('SD Block Device Not Initialized')
            self.blocks = 0
            self.sec_size = 512


    def readblocks(self, n, buf):
        # print("readblocks(%s, %x(%d))" % (n, id(buf), len(buf)))
        # # esp.flash_read((n + self.START_SEC) * self.SEC_SIZE, buf)
        if(len(buf) % self.sec_size):
            return None
        sdmmc.read_blocks(n, int(len(buf)/self.sec_size), buf)

    def writeblocks(self, n, buf):
        # print("writeblocks(%s, %x(%d))" % (n, id(buf), len(buf)))
        #assert len(buf) <= self.SEC_SIZE, len(buf)
        # esp.flash_erase(n + self.START_SEC)
        # esp.flash_write((n + self.START_SEC) * self.SEC_SIZE, buf)
        if(len(buf) % self.sec_size):
            return None
        sdmmc.write_blocks(n, int(len(buf)/self.sec_size), buf)

    def ioctl(self, op, arg):
        # print("ioctl(%d, %r)" % (op, arg))
        # if op == 1:  # BP_INIT
        #     sdmmc.init()
        #     return None
        if op == 4:  # BP_IOCTL_SEC_COUNT
            return self.blocks
        if op == 5:  # BP_IOCTL_SEC_SIZE
            return self.sec_size

# size = esp.flash_size()
# if size < 1024*1024:
#     # flash too small for a filesystem
#     bdev = None
# else:
#     # for now we use a fixed size for the filesystem
#     bdev = FlashBdev(2048 * 1024 // FlashBdev.SEC_SIZE)

sdbdevice = SDBdev()

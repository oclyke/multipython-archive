import multipython as mp
mp.get() # lists all contexts
mp.start('a = 2\nprint(a)',0) # starts a new MicroPython interpreter from a string w/o core affinity
mp.start('sdcard/test_scr.py',1) # starts a new MicroPython interpreter from a file w/o core affinity
mp.start('sdcard/test_scr.py',1,0) # starts a new MicroPython interpreter from a file on core 0




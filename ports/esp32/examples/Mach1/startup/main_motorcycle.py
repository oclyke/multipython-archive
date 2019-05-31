import addressable
import network
import artnet
import time

aled = addressable.controller(addressable.ALED_CONTROLLER, protocol = 0)
fix = addressable.fixture(495, protocol = 0)
aled.add_fixture(fix)
aled.recompute_chain()
aled.start()

stat = addressable.controller(addressable.STAT_CONTROLLER)
stat_fix = stat.fixtures(0)
stat.recompute_chain()
stat.start()

print(mp.start('scr_0_brakelight.py',1,0,0))
print(mp.start('scr_1_signals.py',1,1,0))


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

dmx_layer = fix.add_layer()
stat_dmx_layer = stat_fix.add_layer()

dmx_layer.set(0,[[0x80,0,0,0]])
stat_dmx_layer.set(0,[[0x80,0,0,0]])

dmx_info = artnet.new_layer_artdmx_info(leds = 135, universe = 0, cpl = 3)
dmx_info_1 = artnet.new_layer_artdmx_info(leds = 135, universe = 1, cpl = 3, start_index = 135)
dmx_info_2 = artnet.new_layer_artdmx_info(leds = 135, universe = 2, cpl = 3, start_index = 270)
dmx_info_3 = artnet.new_layer_artdmx_info(leds = 90, universe = 3, cpl = 3, start_index = 405)

stat_dmx_info = artnet.new_layer_artdmx_info(leds = 1, universe = 4, cpl = 3, start_index = 0)

dmx_layer.add_artdmx_info(dmx_info)
dmx_layer.add_artdmx_info(dmx_info_1)
dmx_layer.add_artdmx_info(dmx_info_2)
dmx_layer.add_artdmx_info(dmx_info_3)

stat_dmx_layer.add_artdmx_info(stat_dmx_info)

with open('artnet_use_sta.txt','r') as f:
    use_sta = f.read()

if( use_sta == '1' ):
    with open('artnet_ssid.txt','r') as f:
        ssid = f.read()

    with open('artnet_password.txt','r') as f:
        password = f.read()

    print('Attempting to connect to network:')
    print('\tSSID: ' + str(ssid))
    print('\tPASS: ' + str(password))
    print('')
    print('Change SSID and PASS by editing \'artnet_ssid.txt\' and \'artnet_password.txt\' respectively')

    artnet.start(network.STA_IF)
    sta_if = network.WLAN(network.STA_IF)
    sta_if.active()
    sta_if.connect(ssid,password)

    time.sleep(5)

    print('Network information: (use IP address {first entry} to direct ArtNet packets')
    print(sta_if.ifconfig())
else:
    artnet.start(network.AP_IF)
    ap_if = network.WLAN(network.AP_IF)
    ap_if.active()

    time.sleep(5)

    print('Created Access Point: (use IP address {first entry} to direct ArtNet packets')
    print(ap_if.ifconfig())

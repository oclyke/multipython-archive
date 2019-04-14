from machine import Pin

class sw_apa102:

    def __init__(self, data, clock):
        self.dat = Pin(data, Pin.OUT)
        self.clk = Pin(clock, Pin.OUT)
    
    def send_bites(self, bites):
        for bite in bites:
            for i in range(8):
                if(bite & (0x80 >> i)):
                    self.dat.value(1)
                else:
                    self.dat.value(0)
                
                self.clk.value(0)
                self.clk.value(1)

    def rgba(self, r, g, b, a):
        sequence = [0,0,0,0,a,b,g,r]
        sequence[4] |= 0xE0
        self.send_bites( sequence )

    def rgb(self, r, g, b):
        self.rgba( r, g, b, 255 )

			
    def hsv2rgb(self, h, s, v):
        if s == 0.0: return (v, v, v)
        i = int(h*6.) # XXX assume int() truncates!
        f = (h*6.)-i; p,q,t = v*(1.-s), v*(1.-s*f), v*(1.-s*(1.-f)); i%=6
        if i == 0: return (v, t, p)
        if i == 1: return (q, v, p)
        if i == 2: return (p, v, t)
        if i == 3: return (p, q, v)
        if i == 4: return (t, p, v)
        if i == 5: return (v, p, q)

    def hsva(self, h, s, v, a):
        rgb = self.hsv2rgb(h, s, v)
        self.rgba(int(rgb[0]*255), int(rgb[1]*255), int(rgb[2]*255), int(a*255))

    def hsv(self, h, s, v):
        self.hsva( h, s, v, 1 )

mach1stat = sw_apa102( 5, 33 )






import time
def rainbow_svnt(S,V,n,t):
	while(1):
		for step in range(n):
			H = step/n
			mach1stat.hsv(H,S,V)
			time.sleep_ms(t)

# def rainbow_svn(S,V,n):
# 	rainbow_svnt(S,V,n,10)

# def rainbow_sv(S,V):
# 	rainbow_svn(S,V,360)

# def rainbow():
# 	rainbow_sv(1,1)


# rainbow_svnt(1,0.5,180,30)
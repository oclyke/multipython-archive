from machine import Pin
import time

statMOSI = Pin(5, Pin.OUT)
statSCLK = Pin(33, Pin.OUT)

def send_bytes( bites ):
    for bite in bites:
        for i in range(8):
            if(bite & (0x80 >> i)):
                statMOSI.value(1)
            else:
                statMOSI.value(0)
            
            statSCLK.value(0)
            #delay?
            statSCLK.value(1)
            #delay?

def send_ARGB(A, R, G, B):
    sequence = [0,0,0,0,A,B,G,R]
    sequence[4] |= 0xE0
    send_bytes( sequence )
            
def hsv_to_rgb(h, s, v):
    if s == 0.0: return (v, v, v)
    i = int(h*6.) # XXX assume int() truncates!
    f = (h*6.)-i; p,q,t = v*(1.-s), v*(1.-s*f), v*(1.-s*(1.-f)); i%=6
    if i == 0: return (v, t, p)
    if i == 1: return (q, v, p)
    if i == 2: return (p, v, t)
    if i == 3: return (p, q, v)
    if i == 4: return (t, p, v)
    if i == 5: return (v, p, q)

def send_HSVA(H, S, V, A):
    rgb = hsv_to_rgb(H, S, V)
    send_ARGB(int(A*255), int(rgb[0]*255), int(rgb[1]*255), int(rgb[2]*255))

def send_HSV(H, S, V):
    send_HSVA(H, S, V, 1)

def rainbow_svnt(S,V,n,t):
    while(1):
        for step in range(n):
            H = step/n
            send_HSV(H,S,V)
            time.sleep_ms(t)

def rainbow_svn(S,V,n):
    rainbow_svnt(S,V,n,10)

def rainbow_sv(S,V):
    rainbow_svn(S,V,360)

def rainbow():
    rainbow_sv(1,1)




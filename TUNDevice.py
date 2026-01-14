from pyroute2 import IPRoute
import os
import fcntl
import struct
import config

class TUNDevice:
    TUNSETIFF   = 0x400454ca
    TUNSETOWNER = 0x400454cc
    IFF_TUN     = 0x0001
    IFF_NO_PI   = 0x1000
    
    def __init__(self, deviceName, deviceIP):
        self.ip = IPRoute()
        self.deviceName = deviceName
        self.deviceIP = deviceIP
        self.deleteIfExists()
        self.tun_fd = self.create()
        self.configureInterface()

    def create(self, owner_uid = None):
        tun_fd = os.open('/dev/net/tun', os.O_RDWR)
        ifr = struct.pack('16sH', self.deviceName.encode(), self.IFF_TUN | self.IFF_NO_PI)
        fcntl.ioctl(tun_fd, self.TUNSETIFF, ifr)
        
        if owner_uid is not None:
            fcntl.ioctl(tun_fd, self.TUNSETOWNER, owner_uid)
        return tun_fd

    def deleteIfExists(self):
        try:
            idx = self.ip.link_lookup(ifname = self.deviceName)[0]
            self.ip.link("del", index=idx)
        except IndexError:
            pass 

    def configureInterface(self):
        idx = self.ip.link_lookup(ifname = self.deviceName)[0]
        self.ip.link('set', index = idx, state = 'up')
        self.ip.addr('add', index = idx, address = self.deviceIP, prefixlen = 24)
        self.ip.link('set', index = idx, mtu = config.MTU)
    
    def getfd(self):
        return self.tun_fd
    
    def write(self, data):
        os.write(self.tun_fd, data)

    def read(self):
        return os.read(self.tun_fd, 1501)

    def closeInterface(self):
        os.close(self.tun_fd)

# mimic a python serial port
class PandaJungleSerial(object):
  def __init__(self, panda_jungle, port, baud):
    self.panda_jungle = panda_jungle
    self.port = port
    self.panda_jungle.set_uart_parity(self.port, 0)
    self.panda_jungle.set_uart_baud(self.port, baud)
    self.buf = b""

  def read(self, l=1):
    tt = self.panda_jungle.serial_read(self.port)
    if len(tt) > 0:
      #print "R: ", tt.encode("hex")
      self.buf += tt
    ret = self.buf[0:l]
    self.buf = self.buf[l:]
    return ret

  def write(self, dat):
    #print "W: ", dat.encode("hex")
    #print '  pigeon_send("' + ''.join(map(lambda x: "\\x%02X" % ord(x), dat)) + '");'
    if(isinstance(dat, bytes)):
      return self.panda_jungle.serial_write(self.port, dat)
    else:
      return self.panda_jungle.serial_write(self.port, str.encode(dat))

  def close(self):
    pass



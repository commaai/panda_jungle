# python library to interface with panda jungle

import binascii
import struct
import hashlib
import socket
import usb1
import os
import time
import traceback
import subprocess
from .dfu import PandaJungleDFU
from .flash_release import flash_release
from .update import ensure_st_up_to_date
from .serial import PandaJungleSerial

__version__ = '0.0.1'

BASEDIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../")

DEBUG = os.getenv("PANDAJUNGLEDEBUG") is not None

def build_st(target, mkfile="Makefile"):
  cmd = 'cd %s && make -f %s clean && make -f %s %s >/dev/null' % (os.path.join(BASEDIR, "board"), mkfile, mkfile, target)
  try:
    output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
  except subprocess.CalledProcessError as exception:
    output = exception.output
    returncode = exception.returncode
    raise

def parse_can_buffer(dat):
  ret = []
  for j in range(0, len(dat), 0x10):
    ddat = dat[j:j+0x10]
    f1, f2 = struct.unpack("II", ddat[0:8])
    extended = 4
    if f1 & extended:
      address = f1 >> 3
    else:
      address = f1 >> 21
    dddat = ddat[8:8+(f2&0xF)]
    if DEBUG:
      print("  R %x: %s" % (address, binascii.hexlify(dddat)))
    ret.append((address, f2>>16, dddat, (f2>>4)&0xFF))
  return ret

class PandaJungle(object):
  SERIAL_DEBUG = 0

  REQUEST_IN = usb1.ENDPOINT_IN | usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE
  REQUEST_OUT = usb1.ENDPOINT_OUT | usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE

  HARNESS_ORIENTATION_NONE = 0
  HARNESS_ORIENTATION_1 = 1
  HARNESS_ORIENTATION_2 = 2

  def __init__(self, serial=None, claim=True):
    self._serial = serial
    self._handle = None
    self.connect(claim)

  def close(self):
    self._handle.close()
    self._handle = None

  def connect(self, claim=True, wait=False):
    if self._handle != None:
      self.close()

    context = usb1.USBContext()
    self._handle = None
    self.wifi = False

    while 1:
      try:
        for device in context.getDeviceList(skip_on_error=True):
          #print(device)
          if device.getVendorID() == 0xbbaa and device.getProductID() in [0xddcf, 0xddef]:
            try:
              this_serial = device.getSerialNumber()
            except Exception as e:
              print(e)
              continue
            if self._serial is None or this_serial == self._serial:
              self._serial = this_serial
              print("opening device", self._serial, hex(device.getProductID()))
              time.sleep(1)
              self.bootstub = device.getProductID() == 0xddef
              self.legacy = (device.getbcdDevice() != 0x2300)
              self._handle = device.open()
              if claim:
                self._handle.claimInterface(0)
                #self._handle.setInterfaceAltSetting(0, 0) #Issue in USB stack
              break
      except Exception as e:
        print("exception", e)
        traceback.print_exc()
      if wait == False or self._handle != None:
        break
      context = usb1.USBContext() #New context needed so new devices show up
    assert(self._handle != None)
    print("connected")

  def reset(self, enter_bootstub=False, enter_bootloader=False):
    # reset
    try:
      if enter_bootloader:
        self._handle.controlWrite(PandaJungle.REQUEST_IN, 0xd1, 0, 0, b'')
      else:
        if enter_bootstub:
          self._handle.controlWrite(PandaJungle.REQUEST_IN, 0xd1, 1, 0, b'')
        else:
          self._handle.controlWrite(PandaJungle.REQUEST_IN, 0xd8, 0, 0, b'')
    except Exception:
      pass
    if not enter_bootloader:
      self.reconnect()

  def reconnect(self):
    self.close()
    time.sleep(1.0)
    success = False
    # wait up to 15 seconds
    for i in range(0, 15):
      try:
        self.connect()
        success = True
        break
      except Exception:
        print("reconnecting is taking %d seconds..." % (i+1))
        try:
          dfu = PandaJungleDFU(PandaJungleDFU.st_serial_to_dfu_serial(self._serial))
          dfu.recover()
        except Exception:
          pass
        time.sleep(1.0)
    if not success:
      raise Exception("reconnect failed")

  @staticmethod
  def flash_static(handle, code):
    # confirm flasher is present
    fr = handle.controlRead(PandaJungle.REQUEST_IN, 0xb0, 0, 0, 0xc)
    assert fr[4:8] == b"\xde\xad\xd0\x0d"

    # unlock flash
    print("flash: unlocking")
    handle.controlWrite(PandaJungle.REQUEST_IN, 0xb1, 0, 0, b'')

    # erase sectors 1 through 3
    print("flash: erasing")
    for i in range(1, 4):
      handle.controlWrite(PandaJungle.REQUEST_IN, 0xb2, i, 0, b'')

    # flash over EP2
    STEP = 0x10
    print("flash: flashing")
    for i in range(0, len(code), STEP):
      handle.bulkWrite(2, code[i:i+STEP])

    # reset
    print("flash: resetting")
    try:
      handle.controlWrite(PandaJungle.REQUEST_IN, 0xd8, 0, 0, b'')
    except Exception:
      pass

  def flash(self, fn=None, code=None, reconnect=True):
    print("flash: main version is " + self.get_version())
    if not self.bootstub:
      self.reset(enter_bootstub=True)
    assert(self.bootstub)

    if fn is None and code is None:
      if self.legacy:
        fn = "obj/comma.bin"
        print("building legacy st code")
        build_st(fn, "Makefile.legacy")
      else:
        fn = "obj/panda_jungle.bin"
        print("building panda jungle st code")
        build_st(fn)
      fn = os.path.join(BASEDIR, "board", fn)

    if code is None:
      with open(fn, "rb") as f:
        code = f.read()

    # get version
    print("flash: bootstub version is " + self.get_version())

    # do flash
    PandaJungle.flash_static(self._handle, code)

    # reconnect
    if reconnect:
      self.reconnect()

  def recover(self, timeout=None):
    self.reset(enter_bootloader=True)
    t_start = time.time()
    while len(PandaJungleDFU.list()) == 0:
      print("waiting for DFU...")
      time.sleep(0.1)
      if timeout is not None and (time.time() - t_start) > timeout:
        return False

    dfu = PandaJungleDFU(PandaJungleDFU.st_serial_to_dfu_serial(self._serial))
    dfu.recover()

    # reflash after recover
    self.connect(True, True)
    self.flash()
    return True

  @staticmethod
  def list():
    context = usb1.USBContext()
    ret = []
    try:
      for device in context.getDeviceList(skip_on_error=True):
        if device.getVendorID() == 0xbbaa and device.getProductID() in [0xddcf, 0xddef]:
          try:
            ret.append(device.getSerialNumber())
          except Exception:
            continue
    except Exception:
      pass
    # TODO: detect if this is real
    #ret += ["WIFI"]
    return ret

  def call_control_api(self, msg):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, msg, 0, 0, b'')

  # ******************* control *******************

  def enter_bootloader(self):
    try:
      self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xd1, 0, 0, b'')
    except Exception as e:
      print(e)
      pass

  def get_version(self):
    return self._handle.controlRead(PandaJungle.REQUEST_IN, 0xd6, 0, 0, 0x40).decode('utf8')

  def get_serial(self):
    dat = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xd0, 0, 0, 0x20)
    hashsig, calc_hash = dat[0x1c:], hashlib.sha1(dat[0:0x1c]).digest()[0:4]
    assert(hashsig == calc_hash)
    return [dat[0:0x10], dat[0x10:0x10+10]]

  # ******************* configuration *******************
  def set_can_forwarding(self, from_bus, to_bus):
    # TODO: This feature may not work correctly with saturated buses
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xdd, from_bus, to_bus, b'')

  def set_obd(self, obd):
    # TODO: check panda jungle type
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xdb, int(obd), 0, b'')

  def set_can_loopback(self, enable):
    # set can loopback mode for all buses
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xe5, int(enable), 0, b'')

  def set_can_enable(self, bus_num, enable):
    # sets the can transciever enable pin
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xf4, int(bus_num), int(enable), b'')

  def set_can_speed_kbps(self, bus, speed):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xde, bus, int(speed*10), b'')

  def set_panda_power(self, enabled):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xa0, int(enabled), 0, b'')

  def set_harness_orientation(self, mode):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xa1, int(mode), 0, b'')

  def set_ignition(self, enabled):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xa2, int(enabled), 0, b'')

  # ******************* can *******************

  def can_send_many(self, arr):
    snds = []
    transmit = 1
    extended = 4
    for addr, _, dat, bus in arr:
      assert len(dat) <= 8
      if DEBUG:
        print("  W %x: %s" % (addr, binascii.hexlify(dat)))
      if addr >= 0x800:
        rir = (addr << 3) | transmit | extended
      else:
        rir = (addr << 21) | transmit
      snd = struct.pack("II", rir, len(dat) | (bus << 4)) + dat
      snd = snd.ljust(0x10, b'\x00')
      snds.append(snd)

    while True:
      try:
        #print("DAT: %s"%b''.join(snds).__repr__())
        if self.wifi:
          for s in snds:
            self._handle.bulkWrite(3, s)
        else:
          self._handle.bulkWrite(3, b''.join(snds))
        break
      except (usb1.USBErrorIO, usb1.USBErrorOverflow):
        print("CAN: BAD SEND MANY, RETRYING")

  def can_send(self, addr, dat, bus):
    self.can_send_many([[addr, None, dat, bus]])

  def can_recv(self):
    dat = bytearray()
    while True:
      try:
        dat = self._handle.bulkRead(1, 0x10*256)
        break
      except (usb1.USBErrorIO, usb1.USBErrorOverflow):
        print("CAN: BAD RECV, RETRYING")
        time.sleep(0.1)
    return parse_can_buffer(dat)

  def can_clear(self, bus):
    """Clears all messages from the specified internal CAN ringbuffer as
    though it were drained.

    Args:
      bus (int): can bus number to clear a tx queue, or 0xFFFF to clear the
        global can rx queue.

    """
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xf1, bus, 0, b'')

  # ******************* serial *******************

  def debug_read(self):
    ret = []
    while 1:
      lret = bytes(self._handle.controlRead(PandaJungle.REQUEST_IN, 0xe0, 0, 0, 0x40))
      if len(lret) == 0:
        break
      ret.append(lret)
    return b''.join(ret)

  def debug_clear(self):
    """Clears all messages (tx and rx) from the debug ringbuffer
    as though it were drained.

    """
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xf2, 0, 0, b'')

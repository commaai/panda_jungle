# python library to interface with panda
import os
import sys
import time
import usb1
import struct
import hashlib
import binascii
import warnings
import logging
from functools import wraps
from typing import Optional
from itertools import accumulate

from .base import BaseHandle
from .constants import FW_PATH, McuType
from .dfu import PandaJungleDFU
from .usb import PandaJungleUsbHandle

__version__ = '0.0.10'

# setup logging
LOGLEVEL = os.environ.get('LOGLEVEL', 'INFO').upper()
logging.basicConfig(level=LOGLEVEL, format='%(message)s')

USBPACKET_MAX_SIZE = 0x40
CANPACKET_HEAD_SIZE = 0x6
DLC_TO_LEN = [0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64]
LEN_TO_DLC = {length: dlc for (dlc, length) in enumerate(DLC_TO_LEN)}


def calculate_checksum(data):
  res = 0
  for b in data:
    res ^= b
  return res

def pack_can_buffer(arr):
  snds = [b'']
  for address, _, dat, bus in arr:
    assert len(dat) in LEN_TO_DLC
    #logging.debug("  W 0x%x: 0x%s", address, dat.hex())

    extended = 1 if address >= 0x800 else 0
    data_len_code = LEN_TO_DLC[len(dat)]
    header = bytearray(CANPACKET_HEAD_SIZE)
    word_4b = address << 3 | extended << 2
    header[0] = (data_len_code << 4) | (bus << 1)
    header[1] = word_4b & 0xFF
    header[2] = (word_4b >> 8) & 0xFF
    header[3] = (word_4b >> 16) & 0xFF
    header[4] = (word_4b >> 24) & 0xFF
    header[5] = calculate_checksum(header[:5] + dat)

    snds[-1] += header + dat
    if len(snds[-1]) > 256: # Limit chunks to 256 bytes
      snds.append(b'')

  return snds

def unpack_can_buffer(dat):
  ret = []

  while len(dat) >= CANPACKET_HEAD_SIZE:
    data_len = DLC_TO_LEN[(dat[0]>>4)]

    header = dat[:CANPACKET_HEAD_SIZE]

    bus = (header[0] >> 1) & 0x7
    address = (header[4] << 24 | header[3] << 16 | header[2] << 8 | header[1]) >> 3

    if (header[1] >> 1) & 0x1:
      # returned
      bus += 128
    if header[1] & 0x1:
      # rejected
      bus += 192

    # we need more from the next transfer
    if data_len > len(dat) - CANPACKET_HEAD_SIZE:
      break

    assert calculate_checksum(dat[:(CANPACKET_HEAD_SIZE+data_len)]) == 0, "CAN packet checksum incorrect"

    data = dat[CANPACKET_HEAD_SIZE:(CANPACKET_HEAD_SIZE+data_len)]
    dat = dat[(CANPACKET_HEAD_SIZE+data_len):]

    ret.append((address, 0, data, bus))

  return (ret, dat)

def ensure_health_packet_version(fn):
  @wraps(fn)
  def wrapper(self, *args, **kwargs):
    if self.health_version < self.HEALTH_PACKET_VERSION:
      raise RuntimeError("Panda firmware has outdated health packet definition. Reflash panda firmware.")
    elif self.health_version > self.HEALTH_PACKET_VERSION:
      raise RuntimeError("Panda python library has outdated health packet definition. Update panda python library.")
    return fn(self, *args, **kwargs)
  return wrapper

def ensure_can_packet_version(fn):
  @wraps(fn)
  def wrapper(self, *args, **kwargs):
    if self.can_version < self.CAN_PACKET_VERSION:
      raise RuntimeError("Panda firmware has outdated CAN packet definition. Reflash panda firmware.")
    elif self.can_version > self.CAN_PACKET_VERSION:
      raise RuntimeError("Panda python library has outdated CAN packet definition. Update panda python library.")
    return fn(self, *args, **kwargs)
  return wrapper

def ensure_can_health_packet_version(fn):
  @wraps(fn)
  def wrapper(self, *args, **kwargs):
    if self.can_health_version < self.CAN_HEALTH_PACKET_VERSION:
      raise RuntimeError("Panda firmware has outdated CAN health packet definition. Reflash panda firmware.")
    elif self.can_health_version > self.CAN_HEALTH_PACKET_VERSION:
      raise RuntimeError("Panda python library has outdated CAN health packet definition. Update panda python library.")
    return fn(self, *args, **kwargs)
  return wrapper

class PandaJungle:

  REQUEST_IN = usb1.ENDPOINT_IN | usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE
  REQUEST_OUT = usb1.ENDPOINT_OUT | usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE

  HW_TYPE_UNKNOWN = b'\x00'
  HW_TYPE_V1 = b'\x01'
  HW_TYPE_V2 = b'\x02'

  F4_DEVICES = (HW_TYPE_V1, )
  H7_DEVICES = (HW_TYPE_V2, )

  CAN_PACKET_VERSION = 4
  HEALTH_PACKET_VERSION = 1
  CAN_HEALTH_PACKET_VERSION = 4
  HEALTH_STRUCT = struct.Struct("<I")
  CAN_HEALTH_STRUCT = struct.Struct("<BIBBBBBBBBIIIIIIIHHBBB")

  HARNESS_ORIENTATION_NONE = 0
  HARNESS_ORIENTATION_1 = 1
  HARNESS_ORIENTATION_2 = 2

  def __init__(self, serial: Optional[str] = None, claim: bool = True):
    self._connect_serial = serial

    self._handle: BaseHandle
    self._handle_open = False
    self.can_rx_overflow_buffer = b''

    # connect and set mcu type
    self.connect(claim)

    # reset comms
    self.can_reset_communications()

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.close()

  def close(self):
    if self._handle_open:
      self._handle.close()
      self._handle_open = False

  def connect(self, claim=True, wait=False):
    self.close()

    self._handle = None
    while self._handle is None:
      # try USB first, then SPI
      self._handle, serial, self.bootstub, bcd = self.usb_connect(self._connect_serial, claim=claim)
      if not wait:
        break

    if self._handle is None:
      raise Exception("failed to connect to panda jungle")

    # Some fallback logic to determine panda and MCU type for old bootstubs,
    # since we now support multiple MCUs and need to know which fw to flash.
    # Three cases to consider:
    # A) oldest bootstubs don't have any way to distinguish
    #    MCU or panda type
    # B) slightly newer (~2 weeks after first C3's built) bootstubs
    #    have the panda type set in the USB bcdDevice
    # C) latest bootstubs also implement the endpoint for panda type
    self._bcd_hw_type = None
    ret = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xc1, 0, 0, 0x40)
    missing_hw_type_endpoint = self.bootstub and ret.startswith(b'\xff\x00\xc1\x3e\xde\xad\xd0\x0d')
    if missing_hw_type_endpoint and bcd is not None:
      self._bcd_hw_type = bcd

    # For case A, we assume F4 MCU type, since all H7 pandas should be case B at worst
    self._assume_f4_mcu = (self._bcd_hw_type is None) and missing_hw_type_endpoint

    self._serial = serial
    self._connect_serial = serial
    self._handle_open = True
    self._mcu_type = self.get_mcu_type()
    self.health_version, self.can_version, self.can_health_version = self.get_packets_versions()
    logging.debug("connected")

  @staticmethod
  def usb_connect(serial, claim=True):
    handle, usb_serial, bootstub, bcd = None, None, None, None
    context = usb1.USBContext()
    context.open()
    try:
      for device in context.getDeviceList(skip_on_error=True):
        if device.getVendorID() == 0xbbaa and device.getProductID() in (0xddcf, 0xddef):
          try:
            this_serial = device.getSerialNumber()
          except Exception:
            continue

          if serial is None or this_serial == serial:
            logging.debug("opening device %s %s", this_serial, hex(device.getProductID()))

            usb_serial = this_serial
            bootstub = device.getProductID() == 0xddef
            handle = device.open()
            if sys.platform not in ("win32", "cygwin", "msys", "darwin"):
              handle.setAutoDetachKernelDriver(True)
            if claim:
              handle.claimInterface(0)
              # handle.setInterfaceAltSetting(0, 0)  # Issue in USB stack

            # bcdDevice wasn't always set to the hw type, ignore if it's the old constant
            this_bcd = device.getbcdDevice()
            if this_bcd is not None and this_bcd != 0x2300:
              bcd = bytearray([this_bcd >> 8, ])

            break
    except Exception:
      logging.exception("USB connect error")

    usb_handle = None
    if handle is not None:
      usb_handle = PandaJungleUsbHandle(handle)
    else:
      context.close()

    return usb_handle, usb_serial, bootstub, bcd

  @staticmethod
  def list():
    ret = PandaJungle.usb_list()
    return list(set(ret))

  @staticmethod
  def usb_list():
    ret = []
    try:
      with usb1.USBContext() as context:
        for device in context.getDeviceList(skip_on_error=True):
          if device.getVendorID() == 0xbbaa and device.getProductID() in (0xddcf, 0xddef):
            try:
              serial = device.getSerialNumber()
              if len(serial) == 24:
                ret.append(serial)
              else:
                warnings.warn(f"found device with panda descriptors but invalid serial: {serial}", RuntimeWarning)
            except Exception:
              continue
    except Exception:
      logging.exception("exception while listing pandas")
    return ret

  def reset(self, enter_bootstub=False, enter_bootloader=False, reconnect=True):
    # no response is expected since it resets right away
    timeout = 15000
    try:
      if enter_bootloader:
        self._handle.controlWrite(PandaJungle.REQUEST_IN, 0xd1, 0, 0, b'', timeout=timeout, expect_disconnect=True)
      else:
        if enter_bootstub:
          self._handle.controlWrite(PandaJungle.REQUEST_IN, 0xd1, 1, 0, b'', timeout=timeout, expect_disconnect=True)
        else:
          self._handle.controlWrite(PandaJungle.REQUEST_IN, 0xd8, 0, 0, b'', timeout=timeout, expect_disconnect=True)
    except Exception:
      pass
    if not enter_bootloader and reconnect:
      self.reconnect()

  @property
  def connected(self) -> bool:
    return self._handle_open

  def reconnect(self):
    if self._handle_open:
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
        logging.debug("reconnecting is taking %d seconds...", i + 1)
        try:
          dfu = PandaJungleDFU(self.get_dfu_serial())
          dfu.recover()
        except Exception:
          pass
        time.sleep(1.0)
    if not success:
      raise Exception("reconnect failed")

  @staticmethod
  def flasher_present(handle: BaseHandle) -> bool:
    fr = handle.controlRead(PandaJungle.REQUEST_IN, 0xb0, 0, 0, 0xc)
    return fr[4:8] == b"\xde\xad\xd0\x0d"

  @staticmethod
  def flash_static(handle, code, mcu_type):
    assert mcu_type is not None, "must set valid mcu_type to flash"

    # confirm flasher is present
    assert PandaJungle.flasher_present(handle)

    # determine sectors to erase
    apps_sectors_cumsum = accumulate(mcu_type.config.sector_sizes[1:])
    last_sector = next((i + 1 for i, v in enumerate(apps_sectors_cumsum) if v > len(code)), -1)
    assert last_sector >= 1, "Binary too small? No sector to erase."
    assert last_sector < 7, "Binary too large! Risk of overwriting provisioning chunk."

    # unlock flash
    logging.warning("flash: unlocking")
    handle.controlWrite(PandaJungle.REQUEST_IN, 0xb1, 0, 0, b'')

    # erase sectors
    logging.warning(f"flash: erasing sectors 1 - {last_sector}")
    for i in range(1, last_sector + 1):
      handle.controlWrite(PandaJungle.REQUEST_IN, 0xb2, i, 0, b'')

    # flash over EP2
    STEP = 0x10
    logging.warning("flash: flashing")
    for i in range(0, len(code), STEP):
      handle.bulkWrite(2, code[i:i + STEP])

    # reset
    logging.warning("flash: resetting")
    try:
      handle.controlWrite(PandaJungle.REQUEST_IN, 0xd8, 0, 0, b'', expect_disconnect=True)
    except Exception:
      pass

  def flash(self, fn=None, code=None, reconnect=True):
    if not fn:
      fn = os.path.join(FW_PATH, self._mcu_type.config.app_fn)
    assert os.path.isfile(fn)
    logging.debug("flash: main version is %s", self.get_version())
    if not self.bootstub:
      self.reset(enter_bootstub=True)
    assert(self.bootstub)

    if code is None:
      with open(fn, "rb") as f:
        code = f.read()

    # get version
    logging.debug("flash: bootstub version is %s", self.get_version())

    # do flash
    PandaJungle.flash_static(self._handle, code, mcu_type=self._mcu_type)

    # reconnect
    if reconnect:
      self.reconnect()

  def recover(self, timeout: Optional[int] = 60, reset: bool = True) -> bool:
    dfu_serial = self.get_dfu_serial()

    if reset:
      self.reset(enter_bootstub=True)
      self.reset(enter_bootloader=True)

    if not self.wait_for_dfu(dfu_serial, timeout=timeout):
      return False

    dfu = PandaJungleDFU(dfu_serial)
    dfu.recover()

    # reflash after recover
    self.connect(True, True)
    self.flash()
    return True

  @staticmethod
  def wait_for_dfu(dfu_serial: Optional[str], timeout: Optional[int] = None) -> bool:
    t_start = time.monotonic()
    dfu_list = PandaJungleDFU.list()
    while (dfu_serial is None and len(dfu_list) == 0) or (dfu_serial is not None and dfu_serial not in dfu_list):
      logging.debug("waiting for DFU...")
      time.sleep(0.1)
      if timeout is not None and (time.monotonic() - t_start) > timeout:
        return False
      dfu_list = PandaJungleDFU.list()
    return True

  @staticmethod
  def wait_for_panda_jungle(serial: Optional[str], timeout: int) -> bool:
    t_start = time.monotonic()
    serials = PandaJungle.list()
    while (serial is None and len(serials) == 0) or (serial is not None and serial not in serials):
      logging.debug("waiting for PandaJungle...")
      time.sleep(0.1)
      if timeout is not None and (time.monotonic() - t_start) > timeout:
        return False
      serials = PandaJungle.list()
    return True

  def up_to_date(self) -> bool:
    current = self.get_signature()
    fn = os.path.join(FW_PATH, self.get_mcu_type().config.app_fn)
    expected = PandaJungle.get_signature_from_firmware(fn)
    return (current == expected)

  def call_control_api(self, msg):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, msg, 0, 0, b'')

  # ******************* health *******************

  @ensure_health_packet_version
  def health(self):
    dat = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xd2, 0, 0, self.HEALTH_STRUCT.size)
    a = self.HEALTH_STRUCT.unpack(dat)
    return {
      "uptime": a[0],
    }

  @ensure_can_health_packet_version
  def can_health(self, can_number):
    LEC_ERROR_CODE = {
      0: "No error",
      1: "Stuff error",
      2: "Form error",
      3: "AckError",
      4: "Bit1Error",
      5: "Bit0Error",
      6: "CRCError",
      7: "NoChange",
    }
    dat = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xc2, int(can_number), 0, self.CAN_HEALTH_STRUCT.size)
    a = self.CAN_HEALTH_STRUCT.unpack(dat)
    return {
      "bus_off": a[0],
      "bus_off_cnt": a[1],
      "error_warning": a[2],
      "error_passive": a[3],
      "last_error": LEC_ERROR_CODE[a[4]],
      "last_stored_error": LEC_ERROR_CODE[a[5]],
      "last_data_error": LEC_ERROR_CODE[a[6]],
      "last_data_stored_error": LEC_ERROR_CODE[a[7]],
      "receive_error_cnt": a[8],
      "transmit_error_cnt": a[9],
      "total_error_cnt": a[10],
      "total_tx_lost_cnt": a[11],
      "total_rx_lost_cnt": a[12],
      "total_tx_cnt": a[13],
      "total_rx_cnt": a[14],
      "total_fwd_cnt": a[15],
      "total_tx_checksum_error_cnt": a[16],
      "can_speed": a[17],
      "can_data_speed": a[18],
      "canfd_enabled": a[19],
      "brs_enabled": a[20],
      "canfd_non_iso": a[21],
    }

  # ******************* control *******************

  def get_version(self):
    return self._handle.controlRead(PandaJungle.REQUEST_IN, 0xd6, 0, 0, 0x40).decode('utf8')

  @staticmethod
  def get_signature_from_firmware(fn) -> bytes:
    with open(fn, 'rb') as f:
      f.seek(-128, 2)  # Seek from end of file
      return f.read(128)

  def get_signature(self) -> bytes:
    part_1 = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xd3, 0, 0, 0x40)
    part_2 = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xd4, 0, 0, 0x40)
    return bytes(part_1 + part_2)

  def get_type(self):
    ret = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xc1, 0, 0, 0x40)

    # old bootstubs don't implement this endpoint, see comment in PandaJungle.device
    if self._bcd_hw_type is not None and (ret is None or len(ret) != 1):
      ret = self._bcd_hw_type

    return ret

  # Returns tuple with health packet version and CAN packet/USB packet version
  def get_packets_versions(self):
    dat = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xdd, 0, 0, 3)
    if dat and len(dat) == 3:
      a = struct.unpack("BBB", dat)
      return (a[0], a[1], a[2])
    else:
      return (0, 0, 0)

  def get_mcu_type(self) -> McuType:
    hw_type = self.get_type()
    if hw_type in PandaJungle.F4_DEVICES:
      return McuType.F4
    elif hw_type in PandaJungle.H7_DEVICES:
      return McuType.H7
    else:
      # have to assume F4, see comment in PandaJungle.connect
      if self._assume_f4_mcu:
        return McuType.F4

    raise ValueError(f"unknown HW type: {hw_type}")

  def has_obd(self):
    return self.get_type() in PandaJungle.HAS_OBD

  def is_internal(self):
    return self.get_type() in PandaJungle.INTERNAL_DEVICES

  def get_serial(self):
    """
      Returns the comma-issued dongle ID from our provisioning
    """
    dat = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xd0, 0, 0, 0x20)
    hashsig, calc_hash = dat[0x1c:], hashlib.sha1(dat[0:0x1c]).digest()[0:4]
    assert(hashsig == calc_hash)
    return [dat[0:0x10].decode("utf8"), dat[0x10:0x10 + 10].decode("utf8")]

  def get_usb_serial(self):
    """
      Returns the serial number reported from the USB descriptor;
      matches the MCU UID
    """
    return self._serial

  def get_dfu_serial(self):
    return PandaJungleDFU.st_serial_to_dfu_serial(self._serial, self._mcu_type)

  def get_uid(self):
    """
      Returns the UID from the MCU
    """
    dat = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xc3, 0, 0, 12)
    return binascii.hexlify(dat).decode()

  def get_secret(self):
    return self._handle.controlRead(PandaJungle.REQUEST_IN, 0xd0, 1, 0, 0x10)

  # ******************* configuration *******************

  def set_obd(self, obd):
    # TODO: check panda type
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xdb, int(obd), 0, b'')

  def set_can_loopback(self, enable):
    # set can loopback mode for all buses
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xe5, int(enable), 0, b'')

  def set_can_enable(self, bus_num, enable):
    # sets the can transceiver enable pin
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xf4, int(bus_num), int(enable), b'')

  def set_can_speed_kbps(self, bus, speed):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xde, bus, int(speed * 10), b'')

  def set_can_data_speed_kbps(self, bus, speed):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xf9, bus, int(speed * 10), b'')

  def set_canfd_non_iso(self, bus, non_iso):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xfc, bus, int(non_iso), b'')

  # ******************* can *******************

  # The panda will NAK CAN writes when there is CAN congestion.
  # libusb will try to send it again, with a max timeout.
  # Timeout is in ms. If set to 0, the timeout is infinite.
  CAN_SEND_TIMEOUT_MS = 10

  def can_reset_communications(self):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xc0, 0, 0, b'')

  @ensure_can_packet_version
  def can_send_many(self, arr, timeout=CAN_SEND_TIMEOUT_MS):
    snds = pack_can_buffer(arr)
    while True:
      try:
        for tx in snds:
          while True:
            bs = self._handle.bulkWrite(3, tx, timeout=timeout)
            tx = tx[bs:]
            if len(tx) == 0:
              break
            logging.error("CAN: PARTIAL SEND MANY, RETRYING")
        break
      except (usb1.USBErrorIO, usb1.USBErrorOverflow):
        logging.error("CAN: BAD SEND MANY, RETRYING")

  def can_send(self, addr, dat, bus, timeout=CAN_SEND_TIMEOUT_MS):
    self.can_send_many([[addr, None, dat, bus]], timeout=timeout)

  @ensure_can_packet_version
  def can_recv(self):
    dat = bytearray()
    while True:
      try:
        dat = self._handle.bulkRead(1, 16384) # Max receive batch size + 2 extra reserve frames
        break
      except (usb1.USBErrorIO, usb1.USBErrorOverflow):
        logging.error("CAN: BAD RECV, RETRYING")
        time.sleep(0.1)
    msgs, self.can_rx_overflow_buffer = unpack_can_buffer(self.can_rx_overflow_buffer + dat)
    return msgs

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

  # ****************** Timer *****************
  def get_microsecond_timer(self):
    dat = self._handle.controlRead(PandaJungle.REQUEST_IN, 0xa8, 0, 0, 4)
    return struct.unpack("I", dat)[0]

  # ****************** Phone *****************
  def set_phone_power(self, enabled):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xb3, int(enabled), 0, b'')

  # ****************** Debug *****************
  def set_green_led(self, enabled):
    self._handle.controlWrite(PandaJungle.REQUEST_OUT, 0xf7, int(enabled), 0, b'')

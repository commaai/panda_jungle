import os
import usb1
import struct
import binascii
from typing import List, Optional

from .base import BaseSTBootloaderHandle
from .usb import STBootloaderUSBHandle
from .constants import FW_PATH, McuType

class PandaJungleDFU:
  def __init__(self, dfu_serial: Optional[str]):
    # try USB, then SPI
    handle: Optional[BaseSTBootloaderHandle]
    handle = PandaJungleDFU.usb_connect(dfu_serial)

    if handle is None:
      raise Exception(f"failed to open DFU device {dfu_serial}")

    self._handle: BaseSTBootloaderHandle = handle
    self._mcu_type: McuType = self._handle.get_mcu_type()

  @staticmethod
  def usb_connect(dfu_serial: Optional[str]) -> Optional[STBootloaderUSBHandle]:
    handle = None
    context = usb1.USBContext()
    context.open()
    for device in context.getDeviceList(skip_on_error=True):
      if device.getVendorID() == 0x0483 and device.getProductID() == 0xdf11:
        try:
          this_dfu_serial = device.open().getASCIIStringDescriptor(3)
        except Exception:
          continue

        if this_dfu_serial == dfu_serial or dfu_serial is None:
          handle = STBootloaderUSBHandle(device, device.open())
          break

    return handle

  @staticmethod
  def list() -> List[str]:
    ret = PandaJungleDFU.usb_list()
    return list(set(ret))

  @staticmethod
  def usb_list() -> List[str]:
    dfu_serials = []
    try:
      with usb1.USBContext() as context:
        for device in context.getDeviceList(skip_on_error=True):
          if device.getVendorID() == 0x0483 and device.getProductID() == 0xdf11:
            try:
              dfu_serials.append(device.open().getASCIIStringDescriptor(3))
            except Exception:
              pass
    except Exception:
      pass
    return dfu_serials

  @staticmethod
  def st_serial_to_dfu_serial(st: str, mcu_type: McuType = McuType.F4):
    if st is None or st == "none":
      return None
    uid_base = struct.unpack("H" * 6, bytes.fromhex(st))
    if mcu_type == McuType.H7:
      return binascii.hexlify(struct.pack("!HHH", uid_base[1] + uid_base[5], uid_base[0] + uid_base[4], uid_base[3])).upper().decode("utf-8")
    else:
      return binascii.hexlify(struct.pack("!HHH", uid_base[1] + uid_base[5], uid_base[0] + uid_base[4] + 0xA, uid_base[3])).upper().decode("utf-8")

  def get_mcu_type(self) -> McuType:
    return self._mcu_type

  def reset(self):
    self._handle.jump(self._mcu_type.config.bootstub_address)

  def program_bootstub(self, code_bootstub):
    self._handle.clear_status()
    self._handle.erase_bootstub()
    self._handle.erase_app()
    self._handle.program(self._mcu_type.config.bootstub_address, code_bootstub)

  def recover(self):
    fn = os.path.join(FW_PATH, self._mcu_type.config.bootstub_fn)
    with open(fn, "rb") as f:
      code = f.read()
    self.program_bootstub(code)
    self.reset()

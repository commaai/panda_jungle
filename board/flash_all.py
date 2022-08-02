#!/usr/bin/env python3
import os

from panda_jungle import PandaJungle


if __name__ == "__main__":
  board_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)))
  fw_path = os.path.join(board_dir, 'obj/panda_jungle.bin')

  for s in PandaJungle.list():
    p = PandaJungle(s)
    p.flash(fw_path)

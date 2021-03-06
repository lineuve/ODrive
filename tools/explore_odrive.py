#!/usr/bin/env python3
"""
Load an odrive object to play with in the IPython interactive shell.
"""

import odrive.core
import argparse
import sys
import platform

# Check if IPython is installed
try:
  import IPython
  embed_ipython = True
except:
  embed_ipython = False

  print("Warning: you don't have IPython installed.")
  print("If you want to have an improved interactive console with pretty colors,")
  print("you should install IPython\n")

  # Ensure interactive mode
  if not bool(getattr(sys, 'ps1', sys.flags.interactive)):
    print("You're not running in interactive mode. Run python -i explore_odrive.py")
    print('')
    sys.exit(1)

  # Enable tab complete if possible
  try:
    import readline
    readline.parse_and_bind("tab: complete")
  except:
    sudo_prefix = "" if platform.system() == "Windows" else "sudo "
    print("Warning: could not enable tab-complete. User experience will suffer.\n"
          "Run `{}pip install readline` and then restart this script to fix this."
          .format(sudo_prefix))


# some enums described in the README
# TODO: transmit as part of the JSON
MOTOR_TYPE_HIGH_CURRENT = 0
#MOTOR_TYPE_LOW_CURRENT = 1
MOTOR_TYPE_GIMBAL = 2

CTRL_MODE_VOLTAGE_CONTROL = 0,
CTRL_MODE_CURRENT_CONTROL = 1,
CTRL_MODE_VELOCITY_CONTROL = 2,
CTRL_MODE_POSITION_CONTROL = 3


# Parse arguments
parser = argparse.ArgumentParser(description='Load an odrive object to play with in the IPython interactive shell.')
parser.add_argument("-v", "--verbose", action="store_true",
                    help="print debug information")
group = parser.add_mutually_exclusive_group()
group.add_argument("-d", "--discover", metavar="CHANNELS", action="store",
                    help="Automatically discover ODrives. Takes a comma-separated list (without spaces) "
                    "to indicate which connection types should be considered. Possible values are "
                    "usb and serial. For example \"--discover usb,serial\" indicates "
                    "that USB and serial ports should be scanned for ODrives. "
                    "If none of the below options are specified, --discover usb is assumed.")
group.add_argument("-u", "--usb", metavar="BUS:DEVICE", action="store",
                    help="Specifies the USB port on which the device is connected. "
                    "For example \"001:014\" means bus 001, device 014. The numbers can be obtained "
                    "using `lsusb`.")
group.add_argument("-s", "--serial", metavar="PORT", action="store",
                    help="Specifies the serial port on which the device is connected. "
                    "For example \"/dev/ttyUSB0\". Use `ls /dev/tty*` to find your port name.")
parser.set_defaults(discover="usb")
args = parser.parse_args()

if (args.verbose):
  printer = print
else:
  printer = lambda x: None

# Connect to device
if not args.usb is None:
  try:
    bus = int(args.usb.split(":")[0])
    address = int(args.usb.split(":")[1])
  except (ValueError, IndexError):
    print("the --usb argument must look something like this: \"001:014\"")
    sys.exit(1)
  try:
    my_odrive = odrive.core.open_usb(bus, address, printer=printer)
  except odrive.protocol.DeviceInitException as ex:
    print(str(ex))
    sys.exit(1)
elif not args.serial is None:
  my_odrive = odrive.core.open_serial(args.serial, printer=printer)
else:
  print("Waiting for device...")
  consider_usb = 'usb' in args.discover.split(',')
  consider_serial = 'serial' in args.discover.split(',')
  my_odrive = odrive.core.find_any(consider_usb, consider_serial, printer=printer)
print("Connected!")


print('')
print('ODRIVE EXPLORER')
print('')
print('You can now type "my_odrive." and press <tab>')
print('This will present you with all the properties that you can reference')
print('')
print('For example: "my_odrive.motor0.encoder.pll_pos"')
print('will print the current encoder position on motor 0')
print('and "my_odrive.motor0.pos_setpoint = 10000"')
print('will send motor0 to 10000')
print('')

# If IPython is installed, embed shell, otherwise drop into interactive stock python shell
if embed_ipython:
  IPython.embed()

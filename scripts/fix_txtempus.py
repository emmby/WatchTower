"""
fix_txtempus.py

Removes unnecessary source files from the txtempus library to prevent build errors 
in the native environment, specifically Raspberry Pi specific implementations.
"""
import os
from os.path import join, isfile

Import("env")

# Path to the library in libdeps
# env['PROJECT_LIBDEPS_DIR'] gives .pio/libdeps
# env['PIOENV'] gives the environment name

libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
env_name = env.subst("$PIOENV")
txtempus_dir = join(libdeps_dir, env_name, "txtempus")
files_to_remove = [
    "txtempus.cc",
    "rpi-control.cc",
    "sunxih3-control.cc",
    "hardware-control.cc"
]

for f in files_to_remove:
    src_file = join(txtempus_dir, "src", f)
    if isfile(src_file):
        print(f"Removing {src_file} to avoid conflict/build error")
        os.remove(src_file)

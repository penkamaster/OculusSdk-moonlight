#!/usr/bin/python

import os
import sys
import shutil
import platform
import subprocess

def call( cmdline ):
    p = subprocess.Popen( cmdline.split(), stderr=subprocess.PIPE )
    (out, err) = p.communicate()
    if not p.returncode == 0:
        print ("%s failed, returned %d" % (cmdline[0], p.returncode))
        exit( p.returncode )

def sdk_install_media( mediaDirName ):
    if os.path.exists(mediaDirName):
        installcmd = "adb push " + mediaDirName + " /sdcard/"
        print("Executing: %s " % installcmd)
        call( installcmd )

if __name__ == '__main__':
    sdk_install_media( 'sdcard_SDK' )

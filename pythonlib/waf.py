#!/usr/bin/env python

# to debug a wap script: wap.py <cwd> ...

import os.path
import sys

thisfile = os.path.abspath(os.path.normpath(__file__))
pythonlib = os.path.join(os.path.dirname(os.path.dirname(thisfile)), "pythonlib")
sys.path.insert(0, pythonlib)
wafdir = os.path.join(os.path.dirname(os.path.dirname(thisfile)), "waf-1.6.6")
sys.path.insert(0, wafdir)

import waflib.Context
WAFVERSION = waflib.Context.WAFVERSION 

if __name__ == '__main__':
    import waflib.extras.compat15#PRELUDE
    from waflib import Scripting
    cwd = sys.argv[1]
    sys.argv = sys.argv[1:]
    Scripting.waf_entry_point(cwd, WAFVERSION, wafdir)

import os.path
import sys

thisfile = os.path.abspath(os.path.normpath(__file__))
pythonlib = os.path.join(os.path.dirname(os.path.dirname(thisfile)), "pythonlib")
sys.path.insert(0, pythonlib)

import SCons.Script

sys.argv += ("--no-cache",)
SCons.Script.main()

import os.path
import sys

if sys.platform == 'win32':
    sys.path = [
        os.path.join(sys.prefix, 'Lib', 'site-packages', 'scons-2.1.0.alpha.20101125'),
        os.path.join(sys.prefix, 'Lib', 'site-packages', 'scons'),
        os.path.join(sys.prefix, 'scons-2.1.0.alpha.20101125'),
        os.path.join(sys.prefix, 'scons')] + sys.path;

import SCons.Script

sys.argv += ("--no-cache",)
SCons.Script.main()

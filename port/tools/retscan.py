"""List functions whose callers declare a different narrow return type than the definition (retscan_lib.py).
usage: python port/tools/retscan.py"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retscan_lib import scan, ROOT  # noqa: E402

for fn, raw, files in scan():
    print('%s defined %s; mismatching callers: %s' % (fn, raw, ' '.join(os.path.basename(f) for f in files)))

python 
import sys 
import gdb

# sys.path.append('/home/elco/repos/firmware/tools')
# from prettyprinters import printers

sys.path.append('/home/elco/source/gcc-arm-none-eabi-5_3-2016q1/share/gcc-arm-none-eabi')

# Load the pretty-printers.
from libstdcxx.v6.printers import register_libstdcxx_printers
register_libstdcxx_printers (gdb.current_objfile ())

end

set print pretty on
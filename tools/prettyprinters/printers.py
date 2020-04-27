import gdb.printing
import re
import gdb


def lookup_function(val):
    "Look-up and return a pretty-printer that can print val."
    # Get the type.
    type = val.type

    # If it points to a reference, get the reference.
    if type.code == gdb.TYPE_CODE_REF:
        type = type.target()

    # Get the unqualified type, stripped of typedefs.
    type = type.unqualified().strip_typedefs()

    # Get the type name.
    typename = type.tag

    if typename == None:
        return None

    # Iterate over local dictionary of types to determine
    # if a printer is registered for that type.  Return an
    # instantiation of the printer if found.
    for function in sorted(pretty_printers_dict):
        if function.match(typename):
            return pretty_printers_dict[function](val)

    # Cannot find a pretty printer.  Return None.
    return None


class FixedPointPrinter:
    "Pretty Printer for safe_elastic_fixed_point"

    def __init__(self, val):
        self.val = val

    def to_string(self):
        v = self.val
        while True:
            v_nested = v["_rep"]
            if(v_nested.type.code == gdb.TYPE_CODE_INT):
                raw = float(v_nested)
                m = re.search('.*cnl::elastic_integer<.*cnl::power<(-[0-9]+), 2> >',
                              str(self.val.type.tag))
                scale = 2**(int(m.group(1)))
                scaled = (raw + 0.0) / scale

                return str(scaled)

            if not v_nested:
                break
            v = v_nested

        return None

    def display_hint(self):
        return 'string'


# register the pretty-printer
pretty_printers_dict = {}
pretty_printers_dict[
    re.compile(
        "^cnl::.*elastic_integer<.*cnl::power<-.*, 2> >"
    )
] = FixedPointPrinter
gdb.pretty_printers.append(lookup_function)


#        return "test"  # + str(self.val.fields())
# cnl::_impl::number<cnl::_impl::number<cnl::elastic_integer<23, cnl::_impl::number<int, cnl::wide_tag<31, int, void> > >, cnl::saturated_overflow_tag>, cnl::power<-12, 2> > (base):cnl::_impl::number<cnl::_impl::number<cnl::elastic_integer<23, cnl::_impl::number<int, cnl::wide_tag<31, int, void> > >, cnl::saturated_overflow_tag>, cnl::power<-12, 2> >
# m = re.search(
#     "cnl::scaled_integer<cnl::_impl::number<cnl::elastic_integer<[0-9]+, cnl::_impl::number<int, cnl::wide_tag<[0-9]+, int, void> > >, cnl::saturated_overflow_tag>, cnl::power<-[0-9]+, 2> >", str(self.val.type))
# scale = 2**(-int(m.group(3)))
# scaled = (raw + 0.0) / scale
# return "{0}: {1}".format(raw, scaled)

# def display_hint(self):

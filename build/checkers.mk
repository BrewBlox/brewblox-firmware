# enable coverage for gcc builds.
CFLAGS += -g -O0 -fno-inline
CFLAGS += --coverage

TEST_BUILD=y
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += --coverage -fsanitize=address,undefined

# use asan to detect errors
CFLAGS += -fno-omit-frame-pointer
# LDFLAGS += -fsanitize=address,undefined  <-- moved to module.mk, because particle makefiles seem lose these somewhere

# detect memory errors
CFLAGS += -fsanitize=address

# detect undefined behavior
CFLAGS += -fsanitize=undefined

# try to keep running
# CFLAGS += -fsanitize-recover=all

CFLAGS += -fsanitize=shift
# This option enables checking that the result of a shift operation is not undefined. Note that what exactly is considered undefined differs slightly between C and C++, as well as between ISO C90 and C99, etc. This option has two suboptions, CFLAGS += -fsanitize=shift-base and CFLAGS += -fsanitize=shift-exponent.

CFLAGS += -fsanitize=shift-exponent
# This option enables checking that the second argument of a shift operation is not negative and is smaller than the precision of the promoted first argument.

CFLAGS += -fsanitize=shift-base
# If the second argument of a shift operation is within range, check that the result of a shift operation is not undefined. Note that what exactly is considered undefined differs slightly between C and C++, as well as between ISO C90 and C99, etc.

CFLAGS += -fsanitize=integer-divide-by-zero
# Detect integer division by zero as well as INT_MIN / -1 division.

CFLAGS += -fsanitize=unreachable
# With this option, the compiler turns the __builtin_unreachable call into a diagnostics message call instead. When reaching the __builtin_unreachable call, the behavior is undefined.

CFLAGS += -fsanitize=vla-bound
# This option instructs the compiler to check that the size of a variable length array is positive.

CFLAGS += -fsanitize=null
# This option enables pointer checking. Particularly, the application built with this option turned on will issue an error message when it tries to dereference a NULL pointer, or if a reference (possibly an rvalue reference) is bound to a NULL pointer, or if a method is invoked on an object pointed by a NULL pointer.

CFLAGS += -fsanitize=return
# This option enables return statement checking. Programs built with this option turned on will issue an error message when the end of a non-void function is reached without actually returning a value. This option works in C++ only.

CFLAGS += -fsanitize=signed-integer-overflow
# This option enables signed integer overflow checking. We check that the result of +, *, and both unary and binary - does not overflow in the signed arithmetics. Note, integer promotion rules must be taken into account. That is, the following is not an overflow:
# signed char a = SCHAR_MAX;
# a++;

CFLAGS += -fsanitize=bounds
# This option enables instrumentation of array bounds. Various out of bounds accesses are detected. Flexible array members, flexible array member-like arrays, and initializers of variables with static storage are not instrumented.

# CFLAGS += -fsanitize=bounds-strict
# This option enables strict instrumentation of array bounds. Most out of bounds accesses are detected, including flexible array members and flexible array member-like arrays. Initializers of variables with static storage are not instrumented.

CFLAGS += -fsanitize=alignment
# This option enables checking of alignment of pointers when they are dereferenced, or when a reference is bound to insufficiently aligned target, or when a method or constructor is invoked on insufficiently aligned object.

CFLAGS += -fsanitize=object-size
# This option enables instrumentation of memory references using the __builtin_object_size function. Various out of bounds pointer accesses are detected.

# CFLAGS += -fsanitize=float-divide-by-zero
# Detect floating-point division by zero. Unlike other similar options, CFLAGS += -fsanitize=float-divide-by-zero is not enabled by CFLAGS += -fsanitize=undefined, since floating-point division by zero can be a legitimate way of obtaining infinities and NaNs.

# CFLAGS += -fsanitize=float-cast-overflow
# This option enables floating-point type to integer conversion checking. We check that the result of the conversion does not overflow. Unlike other similar options, CFLAGS += -fsanitize=float-cast-overflow is not enabled by CFLAGS += -fsanitize=undefined. This option does not work well with FE_INVALID exceptions enabled.

# CFLAGS += -fsanitize=nonnull-attribute
# This option enables instrumentation of calls, checking whether null values are not passed to arguments marked as requiring a non-null value by the nonnull function attribute.

# CFLAGS += -fsanitize=returns-nonnull-attribute
# This option enables instrumentation of return statements in functions marked with returns_nonnull function attribute, to detect returning of null values from such functions.

CFLAGS += -fsanitize=bool
# This option enables instrumentation of loads from bool. If a value other than 0/1 is loaded, a run-time error is issued.

CFLAGS += -fsanitize=enum
# This option enables instrumentation of loads from an enum type. If a value outside the range of values for the enum type is loaded, a run-time error is issued.

CFLAGS += -fsanitize=vptr
# This option enables instrumentation of C++ member function calls, member accesses and some conversions between pointers to base and derived classes, to verify the referenced object has the correct dynamic type.

# CFLAGS += -fsanitize=pointer-overflow
# This option enables instrumentation of pointer arithmetics. If the pointer arithmetics overflows, a run-time error is issued.

# CFLAGS += -fsanitize=builtin
# This option enables instrumentation of arguments to selected builtin functions. If an invalid value is passed to such arguments, a run-time error is issued. E.g. passing 0 as the argument to __builtin_ctz or __builtin_clz invokes undefined behavior and is diagnosed by this option.
# While -ftrapv causes traps for signed overflows to be emitted, CFLAGS += -fsanitize=undefined gives a diagnostic message. This currently works only for the C family of languages.

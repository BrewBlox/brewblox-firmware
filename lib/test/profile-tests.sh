valgrind --tool=callgrind --callgrind-out-file=build/callgrind.out build/lib_test_runner --durations yes "$@"
kcachegrind build/callgrind.out
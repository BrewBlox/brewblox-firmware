valgrind --tool=callgrind --callgrind-out-file=build/callgrind.out build/test_runner --durations yes "$@"
kcachegrind build/callgrind.out
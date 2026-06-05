# Compiler options shared by all brpc-based service binaries in this repo.
BRPC_EXAMPLE_COPTS = [
    "-std=c++17",
    "-DNDEBUG",
    "-O2",
    "-Wall",
    "-Wno-unused-variable",
    "-Wno-unused-parameter",
    "-Wno-sign-compare",
    "-Wno-deprecated-declarations",
]

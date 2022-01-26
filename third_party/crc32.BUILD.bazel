package(default_visibility = ["//visibility:public"])

licenses(["notice"])

crc32c_arm64_HDRS = [
    "src/crc32c_arm64.h",
]

crc32c_arm64_SRCS = [
    "src/crc32c_arm64.cc",
]

crc32c_sse42_HDRS = [
    "src/crc32c_sse42.h",
]

crc32c_sse42_SRCS = [
    "src/crc32c_sse42.cc",
]

crc32c_HDRS = [
    "src/crc32c_arm64.h",
    "src/crc32c_arm64_linux_check.h",
    "src/crc32c_internal.h",
    "src/crc32c_prefetch.h",
    "src/crc32c_read_le.h",
    "src/crc32c_round_up.h",
    "src/crc32c_sse42.h",
    "src/crc32c_sse42_check.h",
    "include/crc32c/crc32c.h",
]

crc32c_SRCS = [
    "src/crc32c_portable.cc",
    "src/crc32c.cc",
]

config_setting(
    name = "windows",
    values = {"cpu": "x64_windows"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "linux_x86_64",
    values = {"cpu": "k8"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "darwin",
    values = {"cpu": "darwin"},
    visibility = ["//visibility:public"],
)

sse42_copts = select({
    ":windows": ["/arch:AVX"],
    ":linux_x86_64": ["-msse4.2"],
    ":darwin": ["-msse4.2"],
    "//conditions:default": [],
})

sse42_enabled = select({
    ":windows": "1",
    ":linux_x86_64": "1",
    ":darwin": "1",
    "//conditions:default": "0",
})

genrule(
    name = "generate_config",
    srcs = ["src/crc32c_config.h.in"],
    outs = ["crc32c/crc32c_config.h"],
    cmd = """
sed -e 's/#cmakedefine01/#define/' \
    -e 's/ BYTE_ORDER_BIG_ENDIAN/ BYTE_ORDER_BIG_ENDIAN 0/' \
    -e 's/ HAVE_BUILTIN_PREFETCH/ HAVE_BUILTIN_PREFETCH 0/' \
    -e 's/ HAVE_MM_PREFETCH/ HAVE_MM_PREFETCH 0/' \
    -e 's/ HAVE_SSE42/ HAVE_SSE42 1/' \
    -e 's/ HAVE_ARM64_CRC32C/ HAVE_ARM64_CRC32C 0/' \
    -e 's/ HAVE_STRONG_GETAUXVAL/ HAVE_STRONG_GETAUXVAL 0/' \
    -e 's/ HAVE_WEAK_GETAUXVAL/ HAVE_WEAK_GETAUXVAL 0/' \
    -e 's/ CRC32C_TESTS_BUILT_WITH_GLOG/ CRC32C_TESTS_BUILT_WITH_GLOG 0/' \
    < $< > $@
""",
)

cc_library(
    name = "crc32c",
    srcs = crc32c_SRCS + crc32c_sse42_SRCS + crc32c_arm64_SRCS,
    hdrs = crc32c_HDRS + ["crc32c/crc32c_config.h"],
    deps = [],
    includes = ["include"],
    copts = sse42_copts,
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "com_github_grpc_grpc",
    remote = "https://github.com/grpc/grpc.git",
    # Using Python 3.10 and a version previous to dbe73c9004e483d24168c220cd589fe1824e72bc fails with "Python Configuration Error: Problem getting python include path for /usr/bin/python3"
    commit = "aea02409bb9a60f838e09f422ea04ec36c58c04a",
)

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

git_repository(
    name = "rules_python",
    remote = "https://github.com/bazelbuild/rules_python.git",
    tag = "0.5.0",
)

# proto_library rules implicitly depend on @com_google_protobuf//:protoc,
# which is the proto-compiler.
git_repository(
    name = "com_google_protobuf",
    remote = "https://github.com/protocolbuffers/protobuf.git",
    tag = "v3.19.3",
)

git_repository(
    name = "com_google_absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    tag = "20211102.0",
)

git_repository(
    name = "com_google_googletest",
    remote = "https://github.com/google/googletest.git",
    tag = "release-1.10.0",
)

http_archive(
    name = "com_google_riegeli",
    sha256 = "059af80271b6e62df2662fbf0d1d2724a8eaf881d16459d59d4025132126672c",
    strip_prefix = "riegeli-75aa942e1ddb5830eadac06339cfd4eb740da6f6",
    url = "https://github.com/google/riegeli/archive/75aa942e1ddb5830eadac06339cfd4eb740da6f6.tar.gz",  # 2022-02-17
)

http_archive(
    name = "org_brotli",
    sha256 = "fec5a1d26f3dd102c542548aaa704f655fecec3622a24ec6e97768dcb3c235ff",
    strip_prefix = "brotli-68f1b90ad0d204907beb58304d0bd06391001a4d",
    urls = ["https://github.com/google/brotli/archive/68f1b90ad0d204907beb58304d0bd06391001a4d.zip"],  # 2021-08-18
)

http_archive(
    name = "net_zstd",
    build_file = "//third_party:net_zstd.BUILD.bazel",
    sha256 = "b6c537b53356a3af3ca3e621457751fa9a6ba96daf3aebb3526ae0f610863532",
    strip_prefix = "zstd-1.4.5/lib",
    urls = ["https://github.com/facebook/zstd/archive/v1.4.5.zip"],  # 2020-05-22
)

http_archive(
    name = "snappy",
    build_file = "//third_party:snappy.BUILD.bazel",
    sha256 = "38b4aabf88eb480131ed45bfb89c19ca3e2a62daeb081bdf001cfb17ec4cd303",
    strip_prefix = "snappy-1.1.8",
    urls = ["https://github.com/google/snappy/archive/1.1.8.zip"],  # 2020-01-14
)

http_archive(
    name = "crc32c",
    build_file = "//third_party:crc32.BUILD.bazel",
    sha256 = "338f1d9d95753dc3cdd882dfb6e176bbb4b18353c29c411ebcb7b890f361722e",
    strip_prefix = "crc32c-1.1.0",
    urls = ["https://github.com/google/crc32c/archive/1.1.0.zip"],  # 2019-05-24
)

http_archive(
    name = "zlib",
    build_file = "//third_party:zlib.BUILD.bazel",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = ["http://zlib.net/fossils/zlib-1.2.11.tar.gz"],  # 2017-01-15
)

http_archive(
    name = "highwayhash",
    build_file = "//third_party:highwayhash.BUILD.bazel",
    sha256 = "cf891e024699c82aabce528a024adbe16e529f2b4e57f954455e0bf53efae585",
    strip_prefix = "highwayhash-276dd7b4b6d330e4734b756e97ccfb1b69cc2e12",
    urls = ["https://github.com/google/highwayhash/archive/276dd7b4b6d330e4734b756e97ccfb1b69cc2e12.zip"],  # 2019-02-22
)

http_archive(
    name = "com_google_farmhash",
    build_file = "//third_party:farmhash.BUILD",
    sha256 = "6560547c63e4af82b0f202cb710ceabb3f21347a4b996db565a411da5b17aba0",
    strip_prefix = "farmhash-816a4ae622e964763ca0862d9dbd19324a1eaf45",
    urls = [
        "https://github.com/google/farmhash/archive/816a4ae622e964763ca0862d9dbd19324a1eaf45.tar.gz",
    ],
)

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Include the Sandboxed API dependency if it does not already exist in this
# project. This ensures that this workspace plays well with other external
# dependencies that might use Sandboxed API.
maybe(
    git_repository,
    name = "com_google_sandboxed_api",
    # This example depends on the latest master. In an embedding project, it
    # is advisable to pin Sandboxed API to a specific revision instead.
    commit = "10c04ed42f51dee1fa5f145e86ca3658a3876cfa",  # 2022-02-17
    # branch = "main",
    remote = "https://github.com/google/sandboxed-api.git",
)

# From here on, Sandboxed API files are available. The statements below setup
# transitive dependencies such as Abseil. Like above, those will only be
# included if they don't already exist in the project.
load(
    "@com_google_sandboxed_api//sandboxed_api/bazel:sapi_deps.bzl",
    "sapi_deps",
)

sapi_deps()

# Need to separately setup Protobuf dependencies in order for the build rules
# to work.
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

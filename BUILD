load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

licenses(["notice"])

exports_files(["LICENSE"])

package(
    default_visibility = ["//:__subpackages__"],
)

proto_library(
    name = "contest_problem_proto",
    srcs = ["contest_problem.proto"],
    deps = [
        "@com_google_protobuf//:duration_proto",
    ],
)

py_proto_library(
    name = "contest_problem_py_pb2",
    deps = [":contest_problem_proto"],
)

cc_proto_library(
    name = "contest_problem_cc_proto",
    deps = [":contest_problem_proto"],
)

cc_test(
    name = "load_data_test",
    srcs = ["load_data_test.cc"],
    deps = [
        ":contest_problem_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "print_names",
    srcs = ["print_names.cc"],
    deps = [
        ":contest_problem_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_riegeli//riegeli/bytes:fd_reader",
        "@com_google_riegeli//riegeli/records:record_reader",
    ],
)

py_binary(
    name = "print_names_and_sources",
    srcs = ["print_names_and_sources.py"],
    deps = [
        ":contest_problem_py_pb2",
        "@com_google_riegeli//python/riegeli",
    ],
)

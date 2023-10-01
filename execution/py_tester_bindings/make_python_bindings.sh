#!/bin/bash

bazel build py_tester_extention.so --
cp ../../bazel-bin/execution/py_tester_extention.so code_contests_tester/

python setup.py sdist bdist_wheel
pip install dist/code_contests_tester-0.1-py3-none-any.whl --force-reinstall

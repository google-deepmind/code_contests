#!/bin/bash

bazel build execution/py_tester_extention.so --
cp bazel-bin/execution/py_tester_extention.so execution/py_bindings/code_contests_tester/

cd execution/py_bindings/
python setup.py sdist bdist_wheel
pip install dist/code_contests_tester-0.1-py3-none-any.whl --force-reinstall

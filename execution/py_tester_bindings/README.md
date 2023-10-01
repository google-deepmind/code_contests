# Python binding for running code_contest test harness

* Builds a Python extention for the C++ code
* The build needs Python 3.9


## Execution

```
chmod+x ./make_python_bindings.sh

./make_python_bindings.sh

```

This generates the binary artifacts (.so), wraps them in a Python package and install them with pip

## Test

run `python3.9 test_python_binding.py`


**Note**

Running the test requires Python 3.9.

However, the test itself requires Python3.10, see [bug report](https://github.com/google-deepmind/code_contests/issues/31)

The code itself has 
as a 
the output should be something like :

```
python3.9 test_python_binding.py
[mounts.cc : 415] RAW: Failed to resolve library: libpython3.10.so.1.0
[global_forkclient.cc : 122] RAW: Starting global forkserver
[mounts.cc : 415] RAW: Failed to resolve library: libpython3.10.so.1.0
compilation results:ProgramStatus.Success
OK - Exit code: 0

test-0 :: status=ProgramStatus.Success, pased=True
=====================================================================
hello

=====================================================================

```


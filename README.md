# CodeContests

CodeContests is a competitive programming dataset for machine-learning. This
dataset was used when training
[AlphaCode](https://deepmind.com/blog/article/Competitive-programming-with-AlphaCode).

It consists of programming problems, from a variety of sources:

Site        | URL                         | Source
----------- | --------------------------- | ------
Aizu        | https://judge.u-aizu.ac.jp  | [CodeNet](https://github.com/IBM/Project_CodeNet)
AtCoder     | https://atcoder.jp          | [CodeNet](https://github.com/IBM/Project_CodeNet)
CodeChef    | https://www.codechef.com    | [description2code](https://github.com/ethancaballero/description2code)
Codeforces  | https://codeforces.com      | [description2code](https://github.com/ethancaballero/description2code) and Codeforces
HackerEarth | https://www.hackerearth.com | [description2code](https://github.com/ethancaballero/description2code)

Problems include test cases in the form of paired inputs and outputs, as well as
both correct and incorrect human solutions in a variety of languages.

## Downloading the dataset

[Install the Cloud SDK](https://cloud.google.com/sdk/docs/quickstart), which
provides the `gsutil` utility. You can then download the full data (~3GiB) with,
e.g:

```
gsutil -m cp -r gs://dm-code_contests /tmp
```

The data consists of `ContestProblem` protocol buffers in
[Riegeli](https://github.com/google/riegeli) format. See `contest_problem.proto`
for the protocol buffer definition and documentation of its fields.

The dataset contains three splits:

Split      | Filename
---------- | ----------------------------------------
Training   | `code_contests_train.riegeli-*-of-00128`
Validation | `code_contests_valid.riegeli`
Test       | `code_contests_test.riegeli`

There is example code for iterating over the dataset in C++ (in
`print_names.cc`) and Python (in `print_names_and_sources.py`). For example, you
can print the source and name of each problem in the validation data by
[installing bazel](https://docs.bazel.build/versions/main/install.html) and then
running:

```
bazel run -c opt \
  :print_names_and_sources /tmp/dm-code_contests/code_contests_valid.riegeli
```

Or do the same for the training data with the following command (which will
print around 13000 lines of output):

```
bazel run -c opt \
  :print_names_and_sources /tmp/dm-code_contests/code_contests_train.riegeli*
```

## Executing and evaluating solutions

The `execution` subdirectory contains code for executing a solution and
evaluating whether it solves a problem. `solve_example` demonstrates this
functionality, and can be run with e.g.

```
bazel run -c opt execution:solve_example -- \
  /tmp/dm-code_contests/code_contests_valid.riegeli
```

The execution code defaults to using Python 3.9 and 2.7, located at
`/usr/bin/python3.9` and `/usr/bin/python2.7`, with standard libraries at
`/usr/lib/python3.9` and `/usr/lib/python2.7`. These can be changed with the
flags defined in `py_locations.cc`, for example:

```
bazel run -c opt execution:solve_example -- \
  --valid_path=/tmp/dm-code_contests/code_contests_valid.riegeli \
  --python3_path=/usr/bin/python3.10 --python3_library_paths=/usr/lib/python3.10
```

## Supported platforms

This repository is supported on Linux, compiled with clang.

## Citing this work

If you use this dataset or code, please cite this paper:

```
@article{li2022competition,
  title={Competition-Level Code Generation with AlphaCode},
    author={Li, Yujia and Choi, David and Chung, Junyoung and Kushman, Nate and
    Schrittwieser, Julian and Leblond, R{\'e}mi and Eccles, Tom and
    Keeling, James and Gimeno, Felix and Dal Lago, Agustin and
    Hubert, Thomas and Choy, Peter and de Masson d'Autume, Cyprien and
    Babuschkin, Igor and Chen, Xinyun and Huang, Po-Sen and Welbl, Johannes and
    Gowal, Sven and Cherepanov, Alexey and Molloy, James and
    Mankowitz, Daniel and Sutherland Robson, Esme and Kohli, Pushmeet and
    de Freitas, Nando and Kavukcuoglu, Koray and Vinyals, Oriol},
  journal={arXiv preprint arXiv:2203.07814},
  year={2022}
}
```

## License

The code is licensed under the
[Apache 2.0 License](https://www.apache.org/licenses/LICENSE-2.0).

All non-code materials provided are made available under the terms of the CC BY
4.0 license
([Creative Commons Attribution 4.0 International license](https://creativecommons.org/licenses/by/4.0/legalcode)).

We gratefully acknowledge the contributions of the following:

*   Codeforces materials are sourced from http://codeforces.com.
*   Description2Code materials are sourced from:
    [Description2Code Dataset](https://github.com/ethancaballero/description2code),
    licensed under the
    [MIT open source license](https://opensource.org/licenses/MIT), copyright
    not specified.
*   CodeNet materials are sourced from:
    [Project_CodeNet](https://github.com/IBM/Project_CodeNet), licensed under
    [Apache 2.0](https://www.apache.org/licenses/LICENSE-2.0), copyright not
    specified.

Use of the third-party software, libraries code or data may be governed by
separate terms and conditions or license provisions. Your use of the third-party
software, libraries or code may be subject to any such terms. We make no
representations here with respect to rights or abilities to use any such
materials.

## Disclaimer

This is not an official Google product.

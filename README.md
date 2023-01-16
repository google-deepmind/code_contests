# CodeContests

CodeContests is a competitive programming dataset for machine-learning. This
dataset was used when training
[AlphaCode](https://deepmind.com/blog/article/Competitive-programming-with-AlphaCode). AlphaCode has been published in [Science](https://www.science.org/doi/10.1126/science.abq1158), with a preprint on [arXiv](https://arxiv.org/abs/2203.07814).

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

## Install bazel

First [install bazel](https://docs.bazel.build/versions/main/install.html)
and verify it builds correctly (we only support Linux with clang, but other
platforms might work):

```sh
bazel build -c opt :print_names_and_sources
```

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
  --valid_path=/tmp/dm-code_contests/code_contests_valid.riegeli
```

Note, for the last command you should see one `Compilation failed` and two
`Compilation succeeded`, if you see three `Compilation failed` then there is
likely an issue with the Python version used, please install and try several
ones before reporting a bug.

The execution code defaults to using Python 3.9 and 2.7, located at
`/usr/bin/python3.9` and `/usr/bin/python2.7`, with standard libraries at
`/usr/lib/python3.9` and `/usr/lib/python2.7`. These can be changed with the
flags defined in `py_locations.cc`, for example:

```
bazel run -c opt execution:solve_example -- \
  --valid_path=/tmp/dm-code_contests/code_contests_valid.riegeli \
  --python3_path=/usr/bin/python3.10 --python3_library_paths=/usr/lib/python3.10
```

In Debian/Ubuntu you can install specific Python versions with

```
sudo apt install python3.9 python3.10 python3.11
```

and you can check if you have some version installed by `which` provides output:

```
which python3.11
```

Note that the Python used for building with bazel and for executing inside the sandbox can be different.

### Note on data and sandbox consistency

The incorrect and correct solutions attached to problems are not guaranteed to compile and execute in the exact same way as in their original contest website (for example different compiler versions or flags or different library versions). Some of the solutions will fail compilation, or will produce sandbox violations, especially if they are incorrect.

### FAQ

We recommend running the following before reporting bugs, which wipes out the
bazel state and sometimes fixes transient errors.

```
bazel clean --expunge
rm -rf ~/.cache/bazel
```

## Supported platforms

This repository is supported on Linux, compiled with clang.

People on MacOS have reported this error:
https://github.com/deepmind/code_contests/issues/5

Windows have reported this error:
https://github.com/deepmind/code_contests/issues/9

## Citing this work

If you use this dataset or code, please cite this paper:

```
@article{
  doi:10.1126/science.abq1158,
  author = {Yujia Li  and David Choi  and Junyoung Chung  and Nate Kushman  and Julian Schrittwieser  and R{\'e}mi Leblond  and Tom Eccles  and James Keeling  and Felix Gimeno  and Agustin Dal Lago  and Thomas Hubert  and Peter Choy  and Cyprien de Masson d’Autume  and Igor Babuschkin  and Xinyun Chen  and Po-Sen Huang  and Johannes Welbl  and Sven Gowal  and Alexey Cherepanov  and James Molloy  and Daniel J. Mankowitz  and Esme Sutherland Robson  and Pushmeet Kohli  and Nando de Freitas  and Koray Kavukcuoglu  and Oriol Vinyals },
  title = {Competition-level code generation with AlphaCode},
  journal = {Science},
  volume = {378},
  number = {6624},
  pages = {1092-1097},
  year = {2022},
  doi = {10.1126/science.abq1158},
  URL = {https://www.science.org/doi/abs/10.1126/science.abq1158},
  eprint = {https://www.science.org/doi/pdf/10.1126/science.abq1158},
  abstract = {Programming is a powerful and ubiquitous problem-solving tool. Systems that can assist programmers or even generate programs themselves could make programming more productive and accessible. Recent transformer-based neural network models show impressive code generation abilities yet still perform poorly on more complex tasks requiring problem-solving skills, such as competitive programming problems. Here, we introduce AlphaCode, a system for code generation that achieved an average ranking in the top 54.3\% in simulated evaluations on recent programming competitions on the Codeforces platform. AlphaCode solves problems by generating millions of diverse programs using specially trained transformer-based networks and then filtering and clustering those programs to a maximum of just 10 submissions. This result marks the first time an artificial intelligence system has performed competitively in programming competitions. Computer programming competitions are popular tests among programmers that require critical thinking informed by experience and creating solutions to unforeseen problems, both of which are key aspects of human intelligence but challenging to mimic by machine learning models. Using self-supervised learning and an encoder-decoder transformer architecture, Li et al. developed AlphaCode, a deep-learning model that can achieve approximately human-level performance on the Codeforces platform, which regularly hosts these competitions and attracts numerous participants worldwide (see the Perspective by Kolter). The development of such coding platforms could have a huge impact on programmers’ productivity. It may even change the culture of programming by shifting human work to formulating problems, with machine learning being the main one responsible for generating and executing codes. —YS Modern machine learning systems can achieve average human-level performance in popular competitive programming contests.}}
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

import sys
from code_contests_tester import ProgramStatus, Py3TesterSandboxer, TestOptions

program = """
x = input()
print(x)

"""


def test_binding(python_310_bin_path, python_310_lib_path):
    tester = Py3TesterSandboxer(python_310_bin_path, [python_310_lib_path])
    options = TestOptions()
    options.num_threads = 4
    options.stop_on_first_failure = True


    def compare_func(a,b):
        return a==b

    result = tester.test(program, ["hello"], options, ["hello\n"], compare_func)
    print(f"compilation results:{result.compilation_result.program_status}")
    print(result.compilation_result.sandbox_result)
    print(result.compilation_result.stderr)

    for i, test_res in enumerate(result.test_results):
        print(f"test-{i} :: status={test_res.program_status}, pased={test_res.passed}")
        print("=====================================================================")
        print(test_res.stdout)
        print("=====================================================================")

if __name__ == '__main__':

    print("Usage: python3.9 test_python_binding.py <path to Python3.10 bin> <path to Python3.10 lib>")

    if not len(sys.argv) ==3:
        print("verify you've read the usage")
        exit(1)
    
    print(sys.argv[1])

    print("---")
    print(sys.argv[2])

    test_binding(sys.argv[1], sys.argv[2])




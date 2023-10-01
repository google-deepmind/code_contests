from code_contests_tester import ProgramStatus, Py3TesterSandboxer, TestOptions
tester = Py3TesterSandboxer("/home/ec2-user/.pyenv/versions/3.10.13/bin/python3.10", ["/home/ec2-user/.pyenv/versions/3.10.13/lib"])
options = TestOptions()
options.num_threads = 4
options.stop_on_first_failure = True


def compare_func(a,b): 
    return a==b

program = """
x = input()
print(x)
"""
result = tester.test(program, ["hello"], options, ["hello\n"], compare_func)
print(f"compilation results:{result.compilation_result.program_status}")
print(result.compilation_result.sandbox_result)
print(result.compilation_result.stderr)

for i, test_res in enumerate(result.test_results):
    print(f"test-{i} :: status={test_res.program_status}, pased={test_res.passed}")
    print("=====================================================================")
    print(test_res.stdout)
    print("=====================================================================")



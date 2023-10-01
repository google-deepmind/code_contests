from setuptools import setup, find_packages

setup(
    name="code_contests_tester",
    version="0.1",
    packages=find_packages(),
    package_data={
        'code_contests_tester': ['py_tester_extention.so'],
    },
)

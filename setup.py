from setuptools import Extension, find_packages, setup


sources = [
    "adapters/python/spl/_native.cpp",
    "src/Artifact.cpp",
    "src/PreparedGraph.cpp",
    "src/Parser.cpp",
    "src/parser/Compiler.cpp",
    "src/parser/Lexer.cpp",
    "src/parser/SyntaxParser.cpp",
    "src/spl.cpp",
    "src/runtime/PythonAdapter.cpp",
]

setup(
    name="structured-pipeline-language",
    version="0.1.0",
    package_dir={"": "adapters/python"},
    packages=find_packages("adapters/python"),
    package_data={"spl": ["worker.py"]},
    ext_modules=[Extension("spl._native", sources, include_dirs=["include"], language="c++", extra_compile_args=["-std=c++20"])],
)

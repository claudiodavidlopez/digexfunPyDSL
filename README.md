# digexfunPyDSL

[![CI](https://github.com/claudiodavidlopez/digexfunPyDSL/actions/workflows/ci.yml/badge.svg)](https://github.com/claudiodavidlopez/digexfunPyDSL/actions/workflows/ci.yml)

Call Python functions and methods from PowerFactory DSL models.

This is the library from the paper [Enhancing PowerFactory Dynamic Models with Python for Rapid Prototyping](https://claudiodavidlopez.github.io/pdf/Lopez2019a.pdf) (ISIE 2019).

> [!IMPORTANT]
> **This repository is kept for historical reasons and is no longer maintained.**
> It relies on PowerFactory's `digexfun` DLL interface for user-defined DSL functions. The PowerFactory User Manual (2023 edition) calls the `digexdyn` and `digexfun` interfaces legacy and recommends avoiding dynamic models based on them whenever possible.
> For new work, see [Using Python in PowerFactory today](#using-python-in-powerfactory-today).
>
> The code was cleaned up once in 2026 (bug fixes, CMake build, tests). It is provided as is; issues and pull requests won't be answered.

## Using Python in PowerFactory today

PowerFactory can import a [Functional Mock-up Unit (FMU)](https://fmi-standard.org/) as a dynamic simulation model. FMI is a tool-independent standard for exchanging simulation models, and Python code can be packaged as an FMU. That gives you what this library offered (model logic written in Python, called during a dynamic simulation) through an interface DIgSILENT supports.

1. Write the model as a Python class and export it as a co-simulation FMU with [PythonFMU](https://github.com/NTNU-IHB/PythonFMU) (`pip install pythonfmu`, then `pythonfmu build -f model.py`).
   Third-party packages such as numpy are not bundled into the FMU: the machine running PowerFactory needs a matching Python environment (see PythonFMU's `deploy` command).
2. Import the FMU into PowerFactory.
   See the PowerFactory User Manual, chapter *Models for Dynamic Simulations*, section *Functional Mock-Up Interface*, and DIgSILENT's [FMI example](https://www.digsilent.de/en/faq-reader-powerfactory/do-you-have-an-example-of-a-dynamic-model-based-on-the-functional-mock-up-interface-fmi.html).

Check the manual of your PowerFactory version for which FMI versions and FMU types it accepts.

## How it works

A DSL model calls four functions provided by the DLL. They load a Python callable, set its arguments one by one, call it, and read back its return values one by one: DSL functions take a fixed number of arguments and return a single value.

| DSL function | Purpose |
|---|---|
| `LoadPyFun(argNum, pyFunID)` | Load callable number `pyFunID` from `CALLABLE_REGISTRY`, taking `argNum` arguments. Call it in the model's initialisation. |
| `SetPyFunArg(argVal, argID, pyFunID)` | Set argument number `argID` to `argVal`. |
| `CallPyFun(pyFunID)` | Call the function with the arguments set so far. |
| `GetPyFunRetVal(retValID, pyFunID)` | Get return value number `retValID` from the last call. |

The Python side is a file called `script.py` that defines a list called `CALLABLE_REGISTRY`. A function's `pyFunID` is its index in that list:

```python
import numpy as np

def square_to_polar(re, im):
    return float(np.hypot(re, im)), float(np.arctan2(im, re))

CALLABLE_REGISTRY = [
    square_to_polar,  # pyFunID = 0
]
```

```
! Block inputs: re, im      Block outputs: mag, ang
pyFunID = 0

inc(dummy0) = LoadPyFun(2, pyFunID)     ! initialisation: 2 arguments

dummy1 = SetPyFunArg(re, 0, pyFunID)    ! every time step
dummy2 = SetPyFunArg(im, 1, pyFunID)
dummy3 = CallPyFun(pyFunID)
mag = GetPyFunRetVal(0, pyFunID)
ang = GetPyFunRetVal(1, pyFunID)
```

Details:

- **Arguments and return values** are floating-point numbers. A callable may return a tuple or list of numbers, a single number, or `None`. Anything `float()` accepts counts as a number (int, bool, numpy scalars), except strings.
- **State** lives in Python objects: register a bound method such as `model.step` to keep state between time steps.
- **One ID per block instance.** Each `pyFunID` has its own arguments, so if several blocks call the same function, register it several times and give each block its own ID.
- **Edits take effect on the next run**, without restarting PowerFactory. Loading an ID that is already loaded starts a new run: `script.py` is imported afresh and the previous run's objects are released.
- **Errors** (an invalid ID, a Python exception, a value that isn't a number) are printed to the output window, with the Python traceback, and the simulation is stopped.
- PowerFactory may evaluate a model's equations more than once per time step, so callables should not assume exactly one call per step.

## Installation

Requirements: 64-bit Windows, PowerFactory, Python 3.10 or newer, and to build: [CMake](https://cmake.org/) 3.26 or newer and the MSVC compiler (Visual Studio, or the Build Tools with the *Desktop development with C++* workload).

```powershell
cmake --preset msvc-x64 -DPython3_ROOT_DIR="C:/path/to/python"   # omit if Python is on PATH
cmake --build --preset release
ctest --preset release                                              # optional
```

The DLL is built against Python's stable ABI, so one build works with any installed Python 3.10 or newer.

1. Copy `build/bin/Release/digexfun.dll` into PowerFactory's installation folder. PowerFactory loads all DLLs there whose names start with `digexfun` when it starts.
2. Put `script.py` in the same folder. It is also found through `PYTHONPATH`.
3. Make sure Python's folder (the one containing `python3.dll`) is on `PATH`, and restart PowerFactory.

## Examples

The PowerFactory models (`.pfd`) are the ones used in the paper; they have not been re-checked against current PowerFactory versions.

- [`examples/electronic_load`](examples/electronic_load): the WECC electronic load model written in Python (paper, Section IV-A), responding to a high-impedance fault.
- [`examples/cosimulation`](examples/cosimulation): a co-simulation interface that exchanges variables with an external simulator over ZeroMQ (Section IV-B). Start `eload.py`, then run the simulation. Requires `pyzmq`.

The machine-learning example from the paper (Section IV-C) was removed because its training data was never part of this repository.

## Development

The tests run without PowerFactory. The DLL gets PowerFactory's DSL value stack from `digadm.dll`, so the tests ship a fake `digadm.dll` and a small host program that loads `digexfun.dll` and calls it the way PowerFactory does. They cover the DSL API, error handling, repeated simulation runs, reference counting, calls from other threads, and the electronic load example.

| File | Contents |
|---|---|
| [`src/digexfun.cpp`](src/digexfun.cpp) | PowerFactory side: `Init`, the DSL value stack |
| [`src/dsl_api.cpp`](src/dsl_api.cpp) | `RegisterFunctions` and the four DSL functions |
| [`src/py_bridge.cpp`](src/py_bridge.cpp) | Embedded Python: `script.py`, callables, GIL |
| [`tests/`](tests) | Mock PowerFactory host and GoogleTest suite |

### 2026 cleanup

Compared with the 2019 version: the project builds with CMake (the Visual Studio project file was missing); Python reference-counting bugs and a per-step memory leak are fixed; invalid IDs, unset arguments and early reads no longer crash PowerFactory; Python errors are reported with tracebacks; new simulation runs are detected reliably; calls from any thread are safe; `script.py` is found next to the DLL; and the example scripts work. The DSL interface is unchanged. See the commit history for details.

## How to cite

If you use `digexfunPyDSL` for a publication, you can cite it in IEEE format as

> C. D. López, M. Cvetković and P. Palensky, "Enhancing PowerFactory Dynamic Models with Python for Rapid Prototyping," in *Proceedings of the 28th IEEE International Symposium on Industrial Electronics*, Vancouver, Jun. 2019.

or in bibtex as

```
@inproceedings{lopez_isie,
	title = {Enhancing {PowerFactory} Dynamic Models with {Python} for Rapid Prototyping},
	booktitle = {Proceedings of the 28$^{th}$ IEEE International Symposium on Industrial Electronics},
	author = {L{\'o}pez, C.~D. and Cvetkovi\'{c}, M. and Palensky, P.},
	month = jun,
	year = {2019},
	pages = {1--7},
}
```

## License

See [LICENSE](LICENSE).

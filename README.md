# digexfunPyDSL

A minimal library for enhancing PowerFactory dynamic models with Python.

From the paper [Enhancing PowerFactory Dynamic Models with Python for Rapid Prototyping](../pdf/Lopez2019a.pdf).

# How to cite

If you use `digexfunPyDSL` for a publication, you can cite it in IEEE format as

* C. D. López, M. Cvetković and P. Palensky, "Enhancing PowerFactory Dynamic Models with Python for Rapid Prototyping," in *Proceedings of the 28th IEEE International Symposium on Industrial Electronics*, Vancouver, Jun. 2019.

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

# Compilation

1. Install the Visual Studio version recomended in your PowerFactory user manual.

2. Download the project files to a directory of your choice.

3. Open the project by double-clicking on the digexfun.sln file.

4. Find your Python installation. Inside there should be an `include` and a `libs` directory.

5. In `Solution Explorer`, right click on the project.

6. In the drop-down menu click on `Properties`.

7. In the dialog go to `Configuration Properties > C/C++ > General Additional Include Files`.

8. Enter the path to the `include` directory of your Python installation.

9. In the same dialog go to `Configuration Properties > Linker > General > Additional Library Directories`.

10. Enter the path to the `libs` directory of your Python installation.

11. Click `OK` and build the solution.

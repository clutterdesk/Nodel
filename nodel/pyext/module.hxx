/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

extern "C" {

extern PyModuleDef nodel_module_def;
extern PyObject* nodel_sentinel;
extern PyObject* iter_keys(PyObject*, PyObject*);
extern PyObject* iter_values(PyObject*, PyObject*);
extern PyObject* iter_items(PyObject*, PyObject*);

} // extern C

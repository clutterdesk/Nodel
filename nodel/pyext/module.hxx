/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

extern "C" {

extern PyModuleDef nodel_module_def;
extern PyObject* nodel_sentinel;
extern PyObject* iter_keys(PyObject* arg);
extern PyObject* iter_values(PyObject* arg);
extern PyObject* iter_items(PyObject* arg);

} // extern C

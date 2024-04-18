#pragma once

#include <Python.h>
#include <nodel/core/Object.h>

extern "C" {

extern PyTypeObject NodelObjectType;

#define NodelObject_CheckExact(op) (Py_TYPE(op) == &NodelObjectType)

typedef struct {
    PyObject_HEAD
    nodel::Object obj;
} NodelObject;

extern NodelObject* NodelObject_wrap(const nodel::Object& obj);

} // extern C

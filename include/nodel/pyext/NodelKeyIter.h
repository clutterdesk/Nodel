#pragma once

#include <Python.h>
#include <nodel/core/Object.h>

extern "C" {

extern PyTypeObject NodelKeyIterType;

typedef struct {
    PyObject_HEAD
    nodel::KeyRange range;
    nodel::KeyIterator it;
    nodel::KeyIterator end;
} NodelKeyIter;

} // extern C

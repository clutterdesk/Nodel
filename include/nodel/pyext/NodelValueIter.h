#pragma once

#include <Python.h>
#include <nodel/core/Object.h>

extern "C" {

extern PyTypeObject NodelValueIterType;

typedef struct {
    PyObject_HEAD
    nodel::ValueRange range;
    nodel::ValueIterator it;
    nodel::ValueIterator end;
} NodelValueIter;

} // extern C

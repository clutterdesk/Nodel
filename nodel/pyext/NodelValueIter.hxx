/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <Python.h>
#include <nodel/core/Object.hxx>

extern "C" {

extern PyTypeObject NodelValueIterType;

typedef struct {
    PyObject_HEAD
    nodel::ValueRange range;
    nodel::ValueIterator it;
    nodel::ValueIterator end;
} NodelValueIter;

} // extern C

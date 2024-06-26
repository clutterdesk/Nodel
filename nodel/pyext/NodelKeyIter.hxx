/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <Python.h>
#include <nodel/core/Object.hxx>

extern "C" {

extern PyTypeObject NodelKeyIterType;

typedef struct {
    PyObject_HEAD
    nodel::KeyRange range;
    nodel::KeyIterator it;
    nodel::KeyIterator end;
} NodelKeyIter;

} // extern C

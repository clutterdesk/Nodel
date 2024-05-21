/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <Python.h>
#include <nodel/core/Object.hxx>

extern "C" {

extern PyTypeObject NodelItemIterType;

typedef struct {
    PyObject_HEAD
    nodel::ItemRange range;
    nodel::ItemIterator it;
    nodel::ItemIterator end;
} NodelItemIter;

} // extern C

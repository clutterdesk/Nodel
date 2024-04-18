#pragma once

#include <Python.h>
#include <nodel/core/Object.h>

extern "C" {

extern PyTypeObject NodelItemIterType;

typedef struct {
    PyObject_HEAD
    nodel::ItemRange range;
    nodel::ItemIterator it;
    nodel::ItemIterator end;
} NodelItemIter;

} // extern C

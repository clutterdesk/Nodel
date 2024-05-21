/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <Python.h>
#include <nodel/core/Object.hxx>

using TreeRange = nodel::Object::TreeRange<nodel::Object, nodel::NoPredicate, nodel::NoPredicate>;
using TreeIterator = nodel::TreeIterator<nodel::Object, nodel::NoPredicate, nodel::NoPredicate>;

extern "C" {

extern PyTypeObject NodelTreeIterType;

typedef struct {
    PyObject_HEAD
    TreeRange range;
    TreeIterator it;
    TreeIterator end;
} NodelTreeIter;

} // extern C

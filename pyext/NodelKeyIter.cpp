// Copyright 2024 Robert A. Dunnagan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Python.h>

#include <nodel/pyext/module.h>
#include <nodel/pyext/NodelObject.h>
#include <nodel/pyext/NodelKeyIter.h>
#include <nodel/pyext/support.h>

extern "C" {

using namespace nodel;

static python::Support support;

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static int NodelKeyIter_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelKeyIter* nd_self = (NodelKeyIter*)self;

    std::construct_at<KeyRange>(&nd_self->range);
    std::construct_at<KeyIterator>(&nd_self->it);
    std::construct_at<KeyIterator>(&nd_self->end);

    return 0;
}

static void NodelKeyIter_dealloc(PyObject* self) {
    NodelKeyIter* nd_self = (NodelKeyIter*)self;

    std::destroy_at<KeyRange>(&nd_self->range);
    std::destroy_at<KeyIterator>(&nd_self->it);
    std::destroy_at<KeyIterator>(&nd_self->end);

    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelKeyIter_str(PyObject* self) {
    return PyUnicode_FromString("KeyIterator");
}

static PyObject* NodelKeyIter_repr(PyObject* arg) {
    return NodelKeyIter_str(arg);
}

static PyObject* NodelKeyIter_call(PyObject *self, PyObject *args, PyObject *kwargs) {
    NodelKeyIter* nd_self = (NodelKeyIter*)self;
    if (nd_self->it == nd_self->end) { Py_INCREF(nodel_sentinel); return nodel_sentinel; }
    PyObject* next = support.to_py(*nd_self->it);
    ++nd_self->it;
    return next;
}

static PyObject* NodelKeyIter_iter(PyObject* self) {
    return self;
}

static PyObject* NodelKeyIter_iter_next(PyObject* self) {
    NodelKeyIter* nd_self = (NodelKeyIter*)self;
    if (nd_self->it == nd_self->end) return NULL;  // StopIteration implied
    PyObject* next = support.to_py(*nd_self->it);
    ++nd_self->it;
    return next;
}

//-----------------------------------------------------------------------------
// Type definition
//-----------------------------------------------------------------------------

PyTypeObject NodelKeyIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "nodel.KeyIter",
    .tp_basicsize   = sizeof(NodelKeyIter),
    .tp_doc         = PyDoc_STR("Nodel key iterator"),
    .tp_init        = NodelKeyIter_init,
    .tp_dealloc     = NodelKeyIter_dealloc,
    .tp_repr        = NodelKeyIter_repr,
    .tp_str         = NodelKeyIter_str,
    .tp_iter        = NodelKeyIter_iter,
    .tp_iternext    = NodelKeyIter_iter_next
};

} // extern C

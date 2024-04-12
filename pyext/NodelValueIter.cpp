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
#include <nodel/pyext/NodelValueIter.h>
#include <nodel/pyext/support.h>
#include <nodel/support/logging.h>

extern "C" {

using namespace nodel;

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static int NodelValueIter_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelValueIter* nd_self = (NodelValueIter*)self;

    std::construct_at<ValueRange>(&nd_self->range);
    std::construct_at<ValueIterator>(&nd_self->it);
    std::construct_at<ValueIterator>(&nd_self->end);

    return 0;
}

static void NodelValueIter_dealloc(PyObject* self) {
    NodelValueIter* nd_self = (NodelValueIter*)self;

    std::destroy_at<ValueRange>(&nd_self->range);
    std::destroy_at<ValueIterator>(&nd_self->it);
    std::destroy_at<ValueIterator>(&nd_self->end);

    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelValueIter_str(PyObject* self) {
    return PyUnicode_FromString("ValueIterator");
}

static PyObject* NodelValueIter_repr(PyObject* arg) {
    return NodelValueIter_str(arg);
}

static PyObject* NodelValueIter_call(PyObject *self, PyObject *args, PyObject *kwargs) {
    NodelValueIter* nd_self = (NodelValueIter*)self;
    if (nd_self->it == nd_self->end) { Py_INCREF(nodel_sentinel); return nodel_sentinel; }
    PyObject* next = (PyObject*)NodelObject_wrap(*nd_self->it);
    ++nd_self->it;
    return next;
}

//-----------------------------------------------------------------------------
// Type definition
//-----------------------------------------------------------------------------

PyTypeObject NodelValueIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "nodel.ValueIter",
    .tp_basicsize   = sizeof(NodelValueIter),
    .tp_doc         = PyDoc_STR("Nodel value iterator"),
    .tp_init        = NodelValueIter_init,
    .tp_dealloc     = NodelValueIter_dealloc,
    .tp_repr        = (reprfunc)NodelValueIter_repr,
    .tp_str         = (reprfunc)NodelValueIter_str,
    .tp_call        = (ternaryfunc)NodelValueIter_call,
};

} // extern C

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
#include <nodel/pyext/NodelTreeIter.h>
#include <nodel/pyext/support.h>
#include <nodel/support/logging.h>

extern "C" {

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static int NodelTreeIter_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelTreeIter* nd_self = (NodelTreeIter*)self;

    std::construct_at<TreeRange>(&nd_self->range);
    std::construct_at<TreeIterator>(&nd_self->it);
    std::construct_at<TreeIterator>(&nd_self->end);

    return 0;
}

static PyObject* NodelTreeIter_str(PyObject* self) {
    return PyUnicode_FromString("TreeIterator");
}

static PyObject* NodelTreeIter_repr(PyObject* arg) {
    return NodelTreeIter_str(arg);
}

static PyObject* NodelTreeIter_call(PyObject *self, PyObject *args, PyObject *kwargs) {
    NodelTreeIter* nd_self = (NodelTreeIter*)self;
    if (nd_self->it == nd_self->end) { Py_INCREF(nodel_sentinel); return nodel_sentinel; }
    PyObject* next = (PyObject*)NodelObject_wrap(*nd_self->it);
    ++nd_self->it;
    return next;
}

//-----------------------------------------------------------------------------
// Type definition
//-----------------------------------------------------------------------------

PyTypeObject NodelTreeIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "nodel.TreeIter",
    .tp_basicsize   = sizeof(NodelTreeIter),
    .tp_doc         = PyDoc_STR("Nodel tree iterator"),
    .tp_init        = NodelTreeIter_init,
    .tp_repr        = (reprfunc)NodelTreeIter_repr,
    .tp_str         = (reprfunc)NodelTreeIter_str,
    .tp_call        = (ternaryfunc)NodelTreeIter_call,
};

} // extern C

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
#include <nodel/pyext/NodelKeyIter.h>
#include <nodel/pyext/support.h>

extern "C" {

using namespace nodel;

static PythonSupport support;

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static int NodelKeyIter_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelKeyIter* nself = (NodelKeyIter*)self;

    std::construct_at<KeyRange>(&nself->range);
    std::construct_at<KeyIterator>(&nself->it);
    std::construct_at<KeyIterator>(&nself->end);

    return 0;
}

static PyObject* NodelKeyIter_str(PyObject* self) {
    return PyUnicode_FromString("KeyIterator");
}

static PyObject* NodelKeyIter_repr(PyObject* arg) {
    return NodelKeyIter_str(arg);
}

static PyObject* NodelKeyIter_call(PyObject *self, PyObject *args, PyObject *kwargs) {
    NodelKeyIter* nself = (NodelKeyIter*)self;
    if (nself->it == nself->end) return self;  // self is the sentinel
    PyObject* next = support.to_str(*nself->it);
    ++nself->it;
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
    .tp_repr        = (reprfunc)NodelKeyIter_repr,
    .tp_str         = (reprfunc)NodelKeyIter_str,
    .tp_call        = (ternaryfunc)NodelKeyIter_call,
};

} // extern C

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
#include <nodel/pyext/NodelObject.h>
#include <nodel/pyext/NodelItemIter.h>
#include <nodel/pyext/support.h>

extern "C" {

using namespace nodel;

static PythonSupport support;

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static int NodelItemIter_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelItemIter* nself = (NodelItemIter*)self;

    std::construct_at<ItemRange>(&nself->range);
    std::construct_at<ItemIterator>(&nself->it);
    std::construct_at<ItemIterator>(&nself->end);

    return 0;
}

static PyObject* NodelItemIter_str(PyObject* self) {
    return PyUnicode_FromString("ItemIterator");
}

static PyObject* NodelItemIter_repr(PyObject* arg) {
    return NodelItemIter_str(arg);
}

static PyObject* NodelItemIter_call(PyObject *self, PyObject *args, PyObject *kwargs) {
    NodelItemIter* nself = (NodelItemIter*)self;
    if (nself->it == nself->end) return self;  // self is the sentinel
    auto item = *nself->it;
    PyObject* next = PyTuple_Pack(2, support.to_str(item.first), NodelObject_wrap(item.second));
    ++nself->it;
    return next;
}

//-----------------------------------------------------------------------------
// Type definition
//-----------------------------------------------------------------------------

PyTypeObject NodelItemIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "nodel.ItemIter",
    .tp_basicsize   = sizeof(NodelItemIter),
    .tp_doc         = PyDoc_STR("Nodel item iterator"),
    .tp_init        = NodelItemIter_init,
    .tp_repr        = (reprfunc)NodelItemIter_repr,
    .tp_str         = (reprfunc)NodelItemIter_str,
    .tp_call        = (ternaryfunc)NodelItemIter_call,
};

} // extern C

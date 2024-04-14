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
#include <nodel/pyext/NodelItemIter.h>
#include <nodel/pyext/support.h>

extern "C" {

using namespace nodel;
using RefMgr = python::RefMgr;

static python::Support support;

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static int NodelItemIter_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelItemIter* nd_self = (NodelItemIter*)self;

    std::construct_at<ItemRange>(&nd_self->range);
    std::construct_at<ItemIterator>(&nd_self->it);
    std::construct_at<ItemIterator>(&nd_self->end);

    return 0;
}

static void NodelItemIter_dealloc(PyObject* self) {
    NodelItemIter* nd_self = (NodelItemIter*)self;

    std::destroy_at<ItemRange>(&nd_self->range);
    std::destroy_at<ItemIterator>(&nd_self->it);
    std::destroy_at<ItemIterator>(&nd_self->end);

    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelItemIter_str(PyObject* self) {
    return PyUnicode_FromString("ItemIterator");
}

static PyObject* NodelItemIter_repr(PyObject* arg) {
    return NodelItemIter_str(arg);
}

static PyObject* NodelItemIter_iter(PyObject* self) {
    return self;
}

static PyObject* NodelItemIter_iter_next(PyObject* self) {
    NodelItemIter* nd_self = (NodelItemIter*)self;
    if (nd_self->it == nd_self->end) return NULL;  // StopIteration implied
    auto item = *nd_self->it;
    RefMgr key = support.to_py(item.first);
    RefMgr val = (PyObject*)NodelObject_wrap(item.second);
    PyObject* next = PyTuple_Pack(2, (PyObject*)key, (PyObject*)val);
    ++nd_self->it;
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
    .tp_dealloc     = NodelItemIter_dealloc,
    .tp_repr        = (reprfunc)NodelItemIter_repr,
    .tp_str         = (reprfunc)NodelItemIter_str,
    .tp_iter        = NodelItemIter_iter,
    .tp_iternext    = NodelItemIter_iter_next
};

} // extern C

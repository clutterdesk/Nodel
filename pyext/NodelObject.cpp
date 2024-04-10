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
#include <nodel/pyext/NodelValueIter.h>
#include <nodel/pyext/NodelItemIter.h>
#include <nodel/pyext/support.h>

#include <nodel/format/json.h>

extern "C" {

using namespace nodel;

static PythonSupport support;


//-----------------------------------------------------------------------------
// NodelObject Mapping Interface
//-----------------------------------------------------------------------------

static Py_ssize_t NodelObject_length(PyObject* self) {
    NodelObject* nself = (NodelObject*)self;
    return (Py_ssize_t)nself->obj.size();
}

static PyObject* NodelObject_subscript(PyObject* self, PyObject* key) {
    NodelObject* nself = (NodelObject*)self;
    return (PyObject*)NodelObject_wrap(nself->obj.get(support.to_key(key)));
}

static int NodelObject_ass_sub(PyObject* self, PyObject* key, PyObject* value) {
    NodelObject* nself = (NodelObject*)self;
    if (value == NULL) {
        nself->obj.del(support.to_key(key));
    } else {
        Object nob_value;
        if (Py_IS_TYPE(value, &NodelObjectType)) {
            nob_value = ((NodelObject*)value)->obj;
        } else {
            nob_value = support.to_object(value);
            if (PyErr_Occurred()) return -1;
        }
        nself->obj.set(support.to_key(key), nob_value);
    }
    return 0;
}

static PyMappingMethods NodelObject_as_mapping = {
    .mp_length = (lenfunc)NodelObject_length,
    .mp_subscript = (binaryfunc)NodelObject_subscript,
    .mp_ass_subscript = (objobjargproc)NodelObject_ass_sub
};

//-----------------------------------------------------------------------------
// NodelObject Methods
//-----------------------------------------------------------------------------

static PyObject* NodelObject_root(PyObject* self, PyObject* args) {
}

static PyObject* NodelObject_parent(PyObject* self, PyObject* args) {
}

static PyObject* NodelObject_iter_keys(PyObject* self, PyObject* args) {
    NodelKeyIter* nit = (NodelKeyIter*)NodelKeyIterType.tp_alloc(&NodelKeyIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelKeyIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    NodelObject* nself = (NodelObject*)self;
    nit->range = nself->obj.iter_keys();
    nit->it = nit->range.begin();
    nit->end = nit->range.end();

    return PyCallIter_New((PyObject*)nit, (PyObject*)nit);
}

static PyObject* NodelObject_iter_values(PyObject* self, PyObject* args) {
    NodelValueIter* nit = (NodelValueIter*)NodelValueIterType.tp_alloc(&NodelValueIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelValueIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    NodelObject* nself = (NodelObject*)self;
    nit->range = nself->obj.iter_values();
    nit->it = nit->range.begin();
    nit->end = nit->range.end();

    return PyCallIter_New((PyObject*)nit, (PyObject*)nit);
}

static PyObject* NodelObject_iter_items(PyObject* self, PyObject* args) {
    NodelItemIter* nit = (NodelItemIter*)NodelItemIterType.tp_alloc(&NodelItemIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelItemIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    NodelObject* nself = (NodelObject*)self;
    nit->range = nself->obj.iter_items();
    nit->it = nit->range.begin();
    nit->end = nit->range.end();

    return PyCallIter_New((PyObject*)nit, (PyObject*)nit);
}

static PyMethodDef NodelObject_methods[] = {
    {"root",        (PyCFunction)NodelObject_root,        METH_NOARGS, PyDoc_STR("Returns the root of the tree")},
    {"parent",      (PyCFunction)NodelObject_parent,      METH_NOARGS, PyDoc_STR("Returns the parent object")},
    {"iter_keys",   (PyCFunction)NodelObject_iter_keys,   METH_NOARGS, PyDoc_STR("Returns an iterator over the keys")},
    {"iter_values", (PyCFunction)NodelObject_iter_values, METH_NOARGS, PyDoc_STR("Returns an iterator over the values")},
    {"iter_items",  (PyCFunction)NodelObject_iter_items,  METH_NOARGS, PyDoc_STR("Returns an iterator over the items")},
    {NULL, NULL}
};

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static PyObject* NodelObject_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    return type->tp_alloc(type, 0);
}

static int NodelObject_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelObject* nself = (NodelObject*)self;
    std::construct_at<Object>(&nself->obj);

    PyObject* arg = NULL;  // borrowed reference
    if (!PyArg_ParseTuple(args, "|O", NULL, &arg))
        return -1;

    if (arg != NULL) nself->obj = support.to_object(arg);

    return 0;
}

static void NodelObject_dealloc(PyObject* self) {
    NodelObject* nself = (NodelObject*)self;
    std::destroy_at<Object>(&nself->obj);
    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelObject_str(PyObject* self) {
    NodelObject* nself = (NodelObject*)self;
    return support.to_str(nself->obj);
}

static PyObject* NodelObject_repr(PyObject* arg) {
    return NodelObject_str(arg);
}

static PyObject* NodelObject_richcompare(PyObject* self, PyObject* other, int op) {
    NodelObject* nself = (NodelObject*)self;

    // iterator sentinel comparison
    PyTypeObject* other_typ = Py_TYPE(other);
    if (op == Py_EQ && (other_typ == &NodelKeyIterType || other_typ == &NodelValueIterType || other_typ == &NodelItemIterType))
        return Py_False;

    Object obj = NodelObject_CheckExact(other)? ((NodelObject*)other)->obj: support.to_object(other);
    switch (op) {
        case Py_LT: return (nself->obj <  obj)? Py_True: Py_False;
        case Py_LE: return (nself->obj <= obj)? Py_True: Py_False;
        case Py_EQ: return (nself->obj == obj)? Py_True: Py_False;
        case Py_NE: return (nself->obj != obj)? Py_True: Py_False;
        case Py_GT: return (nself->obj >  obj)? Py_True: Py_False;
        case Py_GE: return (nself->obj >= obj)? Py_True: Py_False;
        default:    break;
    }
    return NULL;
}

//-----------------------------------------------------------------------------
// Type definition
//-----------------------------------------------------------------------------

PyTypeObject NodelObjectType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "nodel.Object",
    .tp_basicsize   = sizeof(NodelObject),
    .tp_doc         = PyDoc_STR("Nodel object"),
    .tp_new         = NodelObject_new,
    .tp_init        = NodelObject_init,
    .tp_dealloc     = (destructor)NodelObject_dealloc,
    .tp_as_mapping  = &NodelObject_as_mapping,
    .tp_richcompare = &NodelObject_richcompare,
    .tp_methods     = NodelObject_methods,
    .tp_repr        = (reprfunc)NodelObject_repr,
    .tp_str         = (reprfunc)NodelObject_str,
};


NodelObject* NodelObject_wrap(const Object& obj) {
    PyObject* module = PyState_FindModule(&nodel_module_def);
    if (module == NULL) return NULL;

    NodelObject* nob = (NodelObject*)PyObject_CallMethodObjArgs(module, PyUnicode_InternFromString("Object"), NULL);
    if (nob == NULL) return NULL;

    nob->obj = obj;
    return nob;
}

} // extern C

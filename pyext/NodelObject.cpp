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
#include <nodel/pyext/NodelTreeIter.h>
#include <nodel/pyext/support.h>

#include <nodel/format/json.h>
#include <nodel/support/logging.h>

extern "C" {

using namespace nodel;
using RefMgr = python::RefMgr;

static python::Support support;


//-----------------------------------------------------------------------------
// Miscellaneous Functions
//-----------------------------------------------------------------------------

static bool require_any_int(const Object& obj) {
    if (!obj.is_any_int()) {
        python::raise_type_error(obj);
        return false;
    }
    return true;
}

static bool require_number(const Object& obj) {
    if (!obj.is_num()) {
        python::raise_type_error(obj);
        return false;
    }
    return true;
}

static bool require_container(const Object& obj) {
    if (!obj.is_container()) {
        python::raise_type_error(obj);
        return false;
    }
    return true;
}

static bool require_list_index(const Object& obj, const Key& key) {
    if (obj.is_type<List>() && !key.is_any_int()) {
        python::raise_type_error(key);
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
// NodelObject Number Protocol
//-----------------------------------------------------------------------------

static PyObject* NodelObject_add(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_subtract(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_multiply(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_remainder(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_divmod(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_power(PyObject* self, PyObject* arg1, PyObject* arg2) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_negative(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_positive(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_absolute(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static int NodelObject_bool(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    return self_obj.to_bool()? 1: 0;
}

static PyObject* NodelObject_invert(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_lshift(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_rshift(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_and(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_xor(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_or(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_int(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (self_obj.is_type<UInt>()) {
        return PyLong_FromUnsignedLongLong(self_obj.as<UInt>());
    } else {
        try {
            return PyLong_FromLongLong(self_obj.to_int());
        } catch (const WrongType& exc) {
            python::raise_type_error(self_obj);
        }
    }
    return NULL;
}

static PyObject* NodelObject_float(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    try {
        return PyFloat_FromDouble(self_obj.to_float());
    } catch (const WrongType& exc) {
        python::raise_type_error(self_obj);
    }
    return NULL;
}

static PyObject* NodelObject_inplace_add(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_subtract(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_multiply(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_remainder(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_power(PyObject* self, PyObject* arg1, PyObject* arg2) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_lshift(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_rshift(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_and(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_xor(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_or(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_floor_divide(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_true_divide(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_floor_divide(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_true_divide(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_index(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_any_int(self_obj)) return NULL;
    if (self_obj.is_type<UInt>()) {
        return PyLong_FromUnsignedLongLong(self_obj.as<UInt>());
    } else {
        try {
            return PyLong_FromLongLong(self_obj.to_int());
        } catch (const WrongType& exc) {
            python::raise_type_error(self_obj);
        }
    }
    return NULL;
}

static PyObject* NodelObject_matrix_multiply(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_matrix_multiply(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}


static PyNumberMethods NodelObject_as_number = {
    .nb_add = NodelObject_add,
    .nb_subtract = NodelObject_subtract,
    .nb_multiply = NodelObject_multiply,
    .nb_remainder = NodelObject_remainder,
    .nb_divmod = NodelObject_divmod,
    .nb_power = NodelObject_power,
    .nb_negative = NodelObject_negative,
    .nb_positive = NodelObject_positive,
    .nb_absolute = NodelObject_absolute,
    .nb_bool = NodelObject_bool,
    .nb_invert = NodelObject_invert,
    .nb_lshift = NodelObject_lshift,
    .nb_rshift = NodelObject_rshift,
    .nb_and = NodelObject_and,
    .nb_xor = NodelObject_xor,
    .nb_or = NodelObject_or,
    .nb_int = NodelObject_int,
    .nb_float = NodelObject_float,
    .nb_inplace_add = NodelObject_inplace_add,
    .nb_inplace_subtract = NodelObject_inplace_subtract,
    .nb_inplace_multiply = NodelObject_inplace_multiply,
    .nb_inplace_remainder = NodelObject_inplace_remainder,
    .nb_inplace_power = NodelObject_inplace_power,
    .nb_inplace_lshift = NodelObject_inplace_lshift,
    .nb_inplace_rshift = NodelObject_inplace_rshift,
    .nb_inplace_and = NodelObject_inplace_and,
    .nb_inplace_xor = NodelObject_inplace_xor,
    .nb_inplace_or = NodelObject_inplace_or,
    .nb_floor_divide = NodelObject_floor_divide,
    .nb_true_divide = NodelObject_true_divide,
    .nb_inplace_floor_divide = NodelObject_inplace_floor_divide,
    .nb_inplace_true_divide = NodelObject_inplace_true_divide,
    .nb_index = NodelObject_index,
    .nb_matrix_multiply = NodelObject_matrix_multiply,
    .nb_inplace_matrix_multiply = NodelObject_inplace_matrix_multiply,
};


//-----------------------------------------------------------------------------
// NodelObject Mapping Protocol
//-----------------------------------------------------------------------------

static Py_ssize_t NodelObject_length(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_container(self_obj)) return NULL;
    return (Py_ssize_t)nd_self->obj.size();
}

static PyObject* NodelObject_subscript(PyObject* self, PyObject* key) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_container(self_obj)) return NULL;
    auto nd_key = support.to_key(key);
    if (!require_list_index(self_obj, nd_key)) return NULL;
    return (PyObject*)NodelObject_wrap(self_obj.get(nd_key));
}

static int NodelObject_ass_sub(PyObject* self, PyObject* key, PyObject* value) {
    // TODO: Write/Clobber exceptions

    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_container(self_obj)) return -1;
    auto nd_key = support.to_key(key);
    if (!require_list_index(self_obj, nd_key)) return -1;
    if (value == NULL) {
        self_obj.del(nd_key);
    } else {
        Object nd_val;
        if (Py_IS_TYPE(value, &NodelObjectType)) {
            nd_val = ((NodelObject*)value)->obj;
        } else {
            nd_val = support.to_object(value);
            if (PyErr_Occurred()) return -1;
        }
        self_obj.set(nd_key, nd_val);
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

static PyObject* NodelObject_is_same(PyObject* self, PyObject* arg) {
    if (!NodelObject_CheckExact(arg)) Py_RETURN_FALSE;
    NodelObject* nd_self = (NodelObject*)self;
    NodelObject* nd_arg = (NodelObject*)arg;
    if (nd_self->obj.is(nd_arg->obj)) Py_RETURN_TRUE; else Py_RETURN_FALSE;
}

static PyObject* NodelObject_root(PyObject* self, PyObject*) {
    NodelObject* nd_self = (NodelObject*)self;
    return (PyObject*)NodelObject_wrap(nd_self->obj.root());
}

static PyObject* NodelObject_parent(PyObject* self, PyObject*) {
    NodelObject* nd_self = (NodelObject*)self;
    return (PyObject*)NodelObject_wrap(nd_self->obj.parent());
}

static PyObject* NodelObject_iter_keys(PyObject* self, PyObject*) {
    NodelKeyIter* nit = (NodelKeyIter*)NodelKeyIterType.tp_alloc(&NodelKeyIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelKeyIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    NodelObject* nd_self = (NodelObject*)self;
    nit->range = nd_self->obj.iter_keys();
    nit->it = nit->range.begin();
    nit->end = nit->range.end();

    return PyCallIter_New((PyObject*)nit, nodel_sentinel);
}

static PyObject* NodelObject_iter_values(PyObject* self, PyObject*) {
    NodelValueIter* nit = (NodelValueIter*)NodelValueIterType.tp_alloc(&NodelValueIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelValueIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    NodelObject* nd_self = (NodelObject*)self;
    nit->range = nd_self->obj.iter_values();
    nit->it = nit->range.begin();
    nit->end = nit->range.end();

    return PyCallIter_New((PyObject*)nit, nodel_sentinel);
}

static PyObject* NodelObject_iter_items(PyObject* self, PyObject*) {
    NodelItemIter* nit = (NodelItemIter*)NodelItemIterType.tp_alloc(&NodelItemIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelItemIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    NodelObject* nd_self = (NodelObject*)self;
    nit->range = nd_self->obj.iter_items();
    nit->it = nit->range.begin();
    nit->end = nit->range.end();

    return PyCallIter_New((PyObject*)nit, nodel_sentinel);
}

static PyObject* NodelObject_iter_tree(PyObject* self, PyObject* args) {
    NodelTreeIter* nit = (NodelTreeIter*)NodelTreeIterType.tp_alloc(&NodelTreeIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelTreeIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    NodelObject* nd_self = (NodelObject*)self;
    nit->range = nd_self->obj.iter_tree();
    nit->it = nit->range.begin();
    nit->end = nit->range.end();

    return PyCallIter_New((PyObject*)nit, nodel_sentinel);
}

static PyObject* NodelObject_key(PyObject* self, PyObject* args) {
    NodelObject* nd_self = (NodelObject*)self;
    return support.to_py(nd_self->obj.key());
}

static PyMethodDef NodelObject_methods[] = {
    {"is_same",     (PyCFunction)NodelObject_is_same,     METH_O,      PyDoc_STR("Returns true if argument is the same object")},
    {"root",        (PyCFunction)NodelObject_root,        METH_NOARGS, PyDoc_STR("Returns the root of the tree")},
    {"parent",      (PyCFunction)NodelObject_parent,      METH_NOARGS, PyDoc_STR("Returns the parent object")},
    {"iter_keys",   (PyCFunction)NodelObject_iter_keys,   METH_NOARGS, PyDoc_STR("Returns an iterator over the keys")},
    {"iter_values", (PyCFunction)NodelObject_iter_values, METH_NOARGS, PyDoc_STR("Returns an iterator over the values")},
    {"iter_items",  (PyCFunction)NodelObject_iter_items,  METH_NOARGS, PyDoc_STR("Returns an iterator over the items")},
    {"iter_tree",   (PyCFunction)NodelObject_iter_tree,   METH_NOARGS, PyDoc_STR("Returns a tree iterator")},
    {"key",         (PyCFunction)NodelObject_key,         METH_NOARGS, PyDoc_STR("Returns the key/index of this object")},
    {NULL, NULL}
};

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static PyObject* NodelObject_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    return type->tp_alloc(type, 0);
}

static int NodelObject_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelObject* nd_self = (NodelObject*)self;
    std::construct_at<Object>(&nd_self->obj);

    PyObject* arg = NULL;  // borrowed reference
    if (!PyArg_ParseTuple(args, "|O", &arg))
        return -1;

    if (arg != NULL) {
        nd_self->obj = support.to_object(arg);
        if (PyErr_Occurred()) return -1;
    }

    return 0;
}

static void NodelObject_dealloc(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    std::destroy_at<Object>(&nd_self->obj);
    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelObject_str(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    return support.to_str(nd_self->obj);
}

static PyObject* NodelObject_repr(PyObject* arg) {
    return NodelObject_str(arg);
}

static PyObject* NodelObject_richcompare(PyObject* self, PyObject* other, int op) {
    NodelObject* nd_self = (NodelObject*)self;

    // iterator sentinel comparison
    if (self == nodel_sentinel) {
        if (other == nodel_sentinel) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } else if (other == nodel_sentinel) {
        Py_RETURN_FALSE;
    }

    Object obj;
    if (NodelObject_CheckExact(other)) {
        obj = ((NodelObject*)other)->obj;
    } else {
        obj = support.to_object(other);
        if (PyErr_Occurred()) return NULL;
    }

    switch (op) {
        case Py_LT: if (nd_self->obj <  obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Py_LE: if (nd_self->obj <= obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Py_EQ: if (nd_self->obj == obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Py_NE: if (nd_self->obj != obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Py_GT: if (nd_self->obj >  obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Py_GE: if (nd_self->obj >= obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        default:    break;
    }
    return NULL;
}

static PyObject* NodelObject_getattro(PyObject* self, PyObject* name) {
    PyObject* val = PyObject_GenericGetAttr(self, name);
    if (val != NULL) return val;

    PyErr_Clear();

    NodelObject* nd_self = (NodelObject*)self;
    Object& self_obj = nd_self->obj;
    try {
        return (PyObject*)NodelObject_wrap(self_obj.get(support.to_key(name)));
    } catch (const WrongType& exc) {
        python::raise_type_error(self_obj);
    }

    return NULL;
}

static int NodelObject_setattro(PyObject* self, PyObject* name, PyObject* val) {
    // TODO: Write/Clobber exceptions

    NodelObject* nd_self = (NodelObject*)self;
    Object& self_obj = nd_self->obj;
    auto nd_key = support.to_key(name);
    try {
        if (NodelObject_CheckExact(val)) {
        } else if (val == Py_None) {
            self_obj.set(nd_key, nil);
        } else {
            Object nd_val = support.to_object(val);
            if (nd_val == nil && PyErr_Occurred()) return -1;
            self_obj.set(nd_key, nd_val);
        }
        return 0;
    } catch (const WrongType& exc) {
        python::raise_type_error(self_obj);
    }

    return -1;
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
    .tp_as_number   = &NodelObject_as_number,
    .tp_as_mapping  = &NodelObject_as_mapping,
    .tp_richcompare = &NodelObject_richcompare,
    .tp_methods     = NodelObject_methods,
    .tp_repr        = (reprfunc)NodelObject_repr,
    .tp_str         = (reprfunc)NodelObject_str,
    .tp_getattro    = &NodelObject_getattro,
    .tp_setattro    = &NodelObject_setattro,
};


//-----------------------------------------------------------------------------
// NodelObject creation
//-----------------------------------------------------------------------------

NodelObject* NodelObject_wrap(const Object& obj) {
    PyObject* module = PyState_FindModule(&nodel_module_def);
    if (module == NULL) return NULL;

    NodelObject* nd_obj = (NodelObject*)PyObject_CallMethodObjArgs(module, PyUnicode_InternFromString("Object"), NULL);
    if (nd_obj == NULL) return NULL;

    nd_obj->obj = obj;
    return nd_obj;
}

} // extern C
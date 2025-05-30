/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <Python.h>

#include <nodel/pyext/module.hxx>
#include <nodel/pyext/NodelObject.hxx>
#include <nodel/pyext/NodelKeyIter.hxx>
#include <nodel/pyext/support.hxx>

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

static PyObject* NodelKeyIter_iter(PyObject* self) {
    Py_INCREF(self);
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
    .tp_dealloc     = NodelKeyIter_dealloc,
    .tp_repr        = NodelKeyIter_repr,
    .tp_str         = NodelKeyIter_str,
    .tp_doc         = PyDoc_STR("Nodel key iterator"),
    .tp_iter        = NodelKeyIter_iter,
    .tp_iternext    = NodelKeyIter_iter_next,
    .tp_init        = NodelKeyIter_init,
};

} // extern C

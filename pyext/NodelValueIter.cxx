/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <Python.h>

#include <nodel/pyext/module.hxx>
#include <nodel/pyext/NodelObject.hxx>
#include <nodel/pyext/NodelValueIter.hxx>
#include <nodel/pyext/support.hxx>
#include <nodel/support/logging.hxx>

extern "C" {

using namespace nodel;

static python::Support support;

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

static PyObject* NodelValueIter_iter(PyObject* self) {
    Py_INCREF(self);
    return self;
}

static PyObject* NodelValueIter_iter_next(PyObject* self) {
    NodelValueIter* nd_self = (NodelValueIter*)self;
    if (nd_self->it == nd_self->end) return NULL;  // StopIteration implied

    PyObject* next = support.prepare_return_value(*nd_self->it);
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
    .tp_dealloc     = NodelValueIter_dealloc,
    .tp_repr        = (reprfunc)NodelValueIter_repr,
    .tp_str         = (reprfunc)NodelValueIter_str,
    .tp_doc         = PyDoc_STR("Nodel value iterator"),
    .tp_iter        = NodelValueIter_iter,
    .tp_iternext    = NodelValueIter_iter_next,
    .tp_init        = NodelValueIter_init,
};

} // extern C

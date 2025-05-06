/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <Python.h>

#include <nodel/pyext/module.hxx>
#include <nodel/pyext/NodelObject.hxx>
#include <nodel/pyext/NodelTreeIter.hxx>
#include <nodel/pyext/support.hxx>
#include <nodel/support/logging.hxx>

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

static void NodelTreeIter_dealloc(PyObject* self) {
    NodelTreeIter* nd_self = (NodelTreeIter*)self;

    std::destroy_at<TreeRange>(&nd_self->range);
    std::destroy_at<TreeIterator>(&nd_self->it);
    std::destroy_at<TreeIterator>(&nd_self->end);

    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelTreeIter_str(PyObject* self) {
    return PyUnicode_FromString("TreeIterator");
}

static PyObject* NodelTreeIter_repr(PyObject* arg) {
    return NodelTreeIter_str(arg);
}

static PyObject* NodelTreeIter_iter(PyObject* self) {
    Py_INCREF(self);
    return self;
}

static PyObject* NodelTreeIter_iter_next(PyObject* self) {
    NodelTreeIter* nd_self = (NodelTreeIter*)self;
    if (nd_self->it == nd_self->end) return NULL;  // StopIteration implied

    PyObject* next = nullptr;
    auto value = *nd_self->it;
    if (value.type() == nodel::Object::ANY) {
        next = value.as<nodel::python::PyOpaque>().m_po;
        Py_INCREF(next);
    } else {
        next = (PyObject*)NodelObject_wrap(value);
    }

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
    .tp_dealloc     = NodelTreeIter_dealloc,
    .tp_repr        = (reprfunc)NodelTreeIter_repr,
    .tp_str         = (reprfunc)NodelTreeIter_str,
    .tp_doc         = PyDoc_STR("Nodel tree iterator"),
    .tp_iter        = NodelTreeIter_iter,
    .tp_iternext    = NodelTreeIter_iter_next,
    .tp_init        = NodelTreeIter_init,
};

} // extern C

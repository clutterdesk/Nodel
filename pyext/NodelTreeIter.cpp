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
    return self;
}

static PyObject* NodelTreeIter_iter_next(PyObject* self) {
    NodelTreeIter* nd_self = (NodelTreeIter*)self;
    if (nd_self->it == nd_self->end) return NULL;  // StopIteration implied
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
    .tp_dealloc     = NodelTreeIter_dealloc,
    .tp_repr        = (reprfunc)NodelTreeIter_repr,
    .tp_str         = (reprfunc)NodelTreeIter_str,
    .tp_doc         = PyDoc_STR("Nodel tree iterator"),
    .tp_iter        = NodelTreeIter_iter,
    .tp_iternext    = NodelTreeIter_iter_next,
    .tp_init        = NodelTreeIter_init,
};

} // extern C

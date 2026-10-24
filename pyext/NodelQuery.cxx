/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <Python.h>

#include <nodel/pyext/module.hxx>
#include <nodel/pyext/NodelObject.hxx>
#include <nodel/pyext/NodelQuery.hxx>
#include <nodel/pyext/support.hxx>

extern "C" {

using namespace nodel;

static python::Support support;

//-----------------------------------------------------------------------------
// Utility functions
//-----------------------------------------------------------------------------

static int NodelQuery_parse(NodelQuery* nd_self, PyObject* spec) {
    if (!PyUnicode_Check(spec)) {
        PyErr_SetString(PyExc_TypeError, "Expected str");
        return -1;
    }

    try {
        nd_self->query = std::make_shared<Query>(python::to_string_view(spec));
    } catch (const parse::SyntaxError& ex) {
        PyErr_SetString(PyExc_ValueError, ex.what());
        return -1;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return -1;
    }

    Py_INCREF(spec);
    Py_XSETREF(nd_self->spec, spec);
    return 0;
}

PyObject* NodelQuery_from_spec(PyObject* spec) {
    NodelQuery* nd_query = (NodelQuery*)NodelQueryType.tp_alloc(&NodelQueryType, 0);
    if (nd_query == NULL) return NULL;
    std::construct_at<std::shared_ptr<Query>>(&nd_query->query);
    if (NodelQuery_parse(nd_query, spec) < 0) {
        Py_DECREF(nd_query);
        return NULL;
    }
    return (PyObject*)nd_query;
}

PyObject* NodelQueryIter_new(const std::shared_ptr<Query>& query, const Object& obj) {
    NodelQueryIter* nit = (NodelQueryIter*)NodelQueryIterType.tp_alloc(&NodelQueryIterType, 0);
    if (nit == NULL) return NULL;
    std::construct_at<std::shared_ptr<Query>>(&nit->query, query);
    std::construct_at<std::optional<QueryEval>>(&nit->eval);

    try {
        nit->eval.emplace(*query, obj);
    } catch (const std::exception& ex) {
        Py_DECREF(nit);
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }

    return (PyObject*)nit;
}

static NodelObject* query_arg(PyObject* arg) {
    if (NodelObject_CheckExact(arg)) return (NodelObject*)arg;
    PyErr_SetString(PyExc_ValueError, "Expected a NodelObject");
    return NULL;
}

//-----------------------------------------------------------------------------
// Query type slots
//-----------------------------------------------------------------------------

static PyObject* NodelQuery_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    NodelQuery* nd_self = (NodelQuery*)type->tp_alloc(type, 0);
    if (nd_self == NULL) return NULL;
    std::construct_at<std::shared_ptr<Query>>(&nd_self->query);
    return (PyObject*)nd_self;
}

static int NodelQuery_init(PyObject* self, PyObject* args, PyObject* kwds) {
    static const char* kwlist[] = {"spec", NULL};
    PyObject* spec;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "U", const_cast<char**>(kwlist), &spec)) return -1;
    return NodelQuery_parse((NodelQuery*)self, spec);
}

static void NodelQuery_dealloc(PyObject* self) {
    NodelQuery* nd_self = (NodelQuery*)self;
    std::destroy_at<std::shared_ptr<Query>>(&nd_self->query);
    Py_XDECREF(nd_self->spec);
    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelQuery_str(PyObject* self) {
    NodelQuery* nd_self = (NodelQuery*)self;
    if (nd_self->spec == NULL) return PyUnicode_FromString("");
    Py_INCREF(nd_self->spec);
    return nd_self->spec;
}

static PyObject* NodelQuery_repr(PyObject* self) {
    NodelQuery* nd_self = (NodelQuery*)self;
    if (nd_self->spec == NULL) return PyUnicode_FromString("nodel.Query()");
    return PyUnicode_FromFormat("nodel.Query(%R)", nd_self->spec);
}

static bool NodelQuery_check_init(NodelQuery* nd_self) {
    if (nd_self->query) return true;
    PyErr_SetString(PyExc_RuntimeError, "Query is not initialized");
    return false;
}

//-----------------------------------------------------------------------------
// Query methods
//-----------------------------------------------------------------------------

constexpr auto iter_doc =
"Evaluate the query lazily, starting from the argument.\n"
"Query.iter(obj) -> iterator";

static PyObject* NodelQuery_iter_method(PyObject* self, PyObject* arg) {
    NodelQuery* nd_self = (NodelQuery*)self;
    if (!NodelQuery_check_init(nd_self)) return NULL;
    NodelObject* nd_obj = query_arg(arg);
    if (nd_obj == NULL) return NULL;
    return NodelQueryIter_new(nd_self->query, nd_obj->obj);
}

constexpr auto list_doc =
"Evaluate the query, starting from the argument, and return a list of the results.\n"
"Query.list(obj) -> list";

static PyObject* NodelQuery_list_method(PyObject* self, PyObject* arg) {
    NodelQuery* nd_self = (NodelQuery*)self;
    if (!NodelQuery_check_init(nd_self)) return NULL;
    NodelObject* nd_obj = query_arg(arg);
    if (nd_obj == NULL) return NULL;

    ObjectList result;
    try {
        result = nd_self->query->eval(nd_obj->obj);
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }

    PyObject* py_list = PyList_New(result.size());
    if (py_list == NULL) return NULL;
    for (size_t i = 0; i < result.size(); ++i) {
        PyObject* item = support.prepare_return_value(result[i]);
        if (item == NULL) {
            Py_DECREF(py_list);
            return NULL;
        }
        PyList_SET_ITEM(py_list, i, item);
    }
    return py_list;
}

static PyMethodDef NodelQuery_methods[] = {
    {"iter", (PyCFunction)NodelQuery_iter_method, METH_O, PyDoc_STR(iter_doc)},
    {"list", (PyCFunction)NodelQuery_list_method, METH_O, PyDoc_STR(list_doc)},
    {NULL}
};

//-----------------------------------------------------------------------------
// Query iterator type slots
//-----------------------------------------------------------------------------

static void NodelQueryIter_dealloc(PyObject* self) {
    NodelQueryIter* nd_self = (NodelQueryIter*)self;
    std::destroy_at<std::optional<QueryEval>>(&nd_self->eval);  // before the Query it refers to
    std::destroy_at<std::shared_ptr<Query>>(&nd_self->query);
    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelQueryIter_str(PyObject* self) {
    return PyUnicode_FromString("QueryIterator");
}

static PyObject* NodelQueryIter_repr(PyObject* arg) {
    return NodelQueryIter_str(arg);
}

static PyObject* NodelQueryIter_iter(PyObject* self) {
    Py_INCREF(self);
    return self;
}

static PyObject* NodelQueryIter_iter_next(PyObject* self) {
    NodelQueryIter* nd_self = (NodelQueryIter*)self;
    if (!nd_self->eval.has_value()) return NULL;  // StopIteration implied

    try {
        Object next = nd_self->eval->next();
        if (next == nil) {
            nd_self->eval.reset();
            return NULL;  // StopIteration implied
        }
        return support.prepare_return_value(next);
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

constexpr auto query_type_doc =
"A compiled query, which can be evaluated repeatedly.\n"
"Query(spec) -> Query\n"
"spec - A query specification.  See nodel.query for the syntax.";

PyTypeObject NodelQueryType = {
    .ob_base        = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "nodel.Query",
    .tp_basicsize   = sizeof(NodelQuery),
    .tp_dealloc     = NodelQuery_dealloc,
    .tp_repr        = (reprfunc)NodelQuery_repr,
    .tp_str         = (reprfunc)NodelQuery_str,
    .tp_flags       = Py_TPFLAGS_DEFAULT,
    .tp_doc         = PyDoc_STR(query_type_doc),
    .tp_methods     = NodelQuery_methods,
    .tp_init        = NodelQuery_init,
    .tp_new         = NodelQuery_new,
};

PyTypeObject NodelQueryIterType = {
    .ob_base        = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "nodel.QueryIter",
    .tp_basicsize   = sizeof(NodelQueryIter),
    .tp_dealloc     = NodelQueryIter_dealloc,
    .tp_repr        = (reprfunc)NodelQueryIter_repr,
    .tp_str         = (reprfunc)NodelQueryIter_str,
    .tp_flags       = Py_TPFLAGS_DEFAULT,
    .tp_doc         = PyDoc_STR("Nodel query iterator"),
    .tp_iter        = NodelQueryIter_iter,
    .tp_iternext    = NodelQueryIter_iter_next,
};

} // extern C

/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#include <Python.h>

#include <nodel/pyext/module.hxx>
#include <nodel/pyext/NodelObject.hxx>
#include <nodel/pyext/NodelKeyIter.hxx>
#include <nodel/pyext/NodelValueIter.hxx>
#include <nodel/pyext/NodelItemIter.hxx>
#include <nodel/pyext/NodelTreeIter.hxx>
#include <nodel/pyext/support.hxx>
#include <nodel/support/intern.hxx>
#include <nodel/parser/json.hxx>
#include <nodel/core.hxx>
#include <nodel/filesystem.hxx>

#ifdef PYNODEL_ROCKSDB
#include <nodel/kvdb.hxx>
#endif

NODEL_INIT_CORE;
NODEL_INIT_FILESYSTEM;

using namespace nodel;
using RefMgr = python::RefMgr;

extern "C" {

PyObject* nodel_sentinel = nullptr;

static python::Support support;

//-----------------------------------------------------------------------------
// Utility functions
//-----------------------------------------------------------------------------

inline
NodelObject* wrap(PyObject* mod, const Object& obj) {
    NodelObject* nd_obj = (NodelObject*)PyObject_CallMethodObjArgs(mod, PyUnicode_InternFromString("Object"), NULL);
    if (nd_obj != NULL) nd_obj->obj = obj;
    return nd_obj;
}

inline
NodelObject* as_nodel_object(PyObject* arg) {
    return NodelObject_CheckExact(arg)? (NodelObject*)arg: NULL;
}

PyObject* iter_keys(PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    NodelKeyIter* nit = (NodelKeyIter*)NodelKeyIterType.tp_alloc(&NodelKeyIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelKeyIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = nd_self->obj.iter_keys();
        nit->it = nit->range.begin();
        nit->end = nit->range.end();

        Py_INCREF(nit);
        return (PyObject*)nit;
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

//-----------------------------------------------------------------------------
// Module methods
//-----------------------------------------------------------------------------

constexpr auto mod_bind_doc = "Bind object with URI string/dict\n"
                              "bind(uri) -> Object\n"
                              "uri - A URI string with a registered scheme";

static PyObject* mod_bind(PyObject* mod, PyObject* arg) {
    try {
        URI uri{support.to_object(arg)};
        return (PyObject*)wrap(mod, bind(uri));
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto mod_from_json_doc = "Parse JSON into an Object\n"
                                   "from_json(json) -> Object";

static PyObject* mod_from_json(PyObject* mod, PyObject* arg) {
    Py_ssize_t size;
    auto c_str = PyUnicode_AsUTF8AndSize(arg, &size);
    if (c_str == NULL) return NULL;

    RefMgr po = PyObject_CallMethodObjArgs(mod, PyUnicode_InternFromString("Object"), NULL);
    if (po == NULL) return NULL;

    NodelObject* obj = (NodelObject*)po.get();
    std::string_view spec{c_str, (std::string_view::size_type)size};
    obj->obj = json::parse(spec);
    if (!obj->obj.is_valid()) {
        PyErr_SetString(PyExc_ValueError, obj->obj.to_str().data());
        return NULL;
    }

    return po.get_clear();
}

constexpr auto clear_method_doc =
"Removes all elements from a container.\n"
"nodel.clear(obj) -> None\n"
"This method does nothing if the object is not a container.";

static PyObject* mod_clear(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    nd_self->obj.clear();
    Py_RETURN_NONE;
}

constexpr auto del_from_parent_method_doc =
"Removes this object from its parent container.\n"
"nodel.clear(obj) -> None\n"
"This method does nothing if the object does not have a parent.";

static PyObject* mod_del_from_parent(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    nd_self->obj.del_from_parent();
    Py_RETURN_NONE;
}

constexpr auto root_method_doc =
"Returns the root of the tree containing the argument.\n"
"nodel.root(obj) -> Object\n"
"Returns the root of the tree, which may be the argument.";

static PyObject* mod_root(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    return (PyObject*)wrap(mod, nd_self->obj.root());
}

constexpr auto parent_method_doc =
"Returns the parent of the argument.\n"
"nodel.parent(obj) -> Object\n"
"If the argument is the root of the tree, then nil is returned.\n"
"Returns the parent of the argument, which may be nil.";

static PyObject* mod_parent(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    return (PyObject*)wrap(mod, nd_self->obj.parent());
}

constexpr auto iter_keys_method_doc =
"Returns an iterator over the keys of the argument.\n"
"nodel.iter_keys(obj) -> iterator\n"
"If the argument is any kind of map, then this method returns an iterator over\n"
"keys in the map.\n"
"If the argument is a list, then this method returns an iterator over the\n"
"indices of the list - similar to the `enumerate` builtin function.\n"
"Otherwise, a RuntimeError exception is raised.\n"
"Returns an iterator over the keys of the argument.";

static PyObject* mod_iter_keys(PyObject* mod, PyObject* arg) {
    return iter_keys(arg);
}

constexpr auto iter_values_method_doc =
"Returns an iterator over the values (Object instances) of the argument.\n"
"nodel.iter_values(obj) -> iterator\n"
"If the argument is any kind of map, then this method returns an iterator over\n"
"values in the map.\n"
"If the argument is a list, then this method returns an iterator over the\n"
"elements in the list.\n"
"Otherwise, a RuntimeError exception is raised.\n"
"Returns an iterator over the values of the argument.";

static PyObject* mod_iter_values(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    NodelValueIter* nit = (NodelValueIter*)NodelValueIterType.tp_alloc(&NodelValueIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelValueIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = nd_self->obj.iter_values();
        nit->it = nit->range.begin();
        nit->end = nit->range.end();

        Py_INCREF(nit);
        return (PyObject*)nit;
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto iter_items_method_doc =
"Returns an iterator over the items of the argument.\n"
"nodel.iter_items(obj) -> iterator\n"
"If the argument is any kind of map, then this method returns an iterator over\n"
"key/value pairs in the map.\n"
"If the argument is a list, then this method returns an iterator over the\n"
"index/value pairs in the list, similar to the `enumerate` builtin function.\n"
"Otherwise, a RuntimeError exception is raised.\n"
"Returns an iterator over the items of the argument.";

static PyObject* mod_iter_items(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    NodelItemIter* nit = (NodelItemIter*)NodelItemIterType.tp_alloc(&NodelItemIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelItemIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = nd_self->obj.iter_items();
        nit->it = nit->range.begin();
        nit->end = nit->range.end();

        Py_INCREF(nit);
        return (PyObject*)nit;
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto iter_tree_method_doc =
"Returns an iterator of the sub-tree whose root is the argument.\n"
"nodel.iter_tree(obj) -> iterator\n"
"A tree iterator iterates over the values (instances of Object) throughout\n"
"the entire tree whose root is this object. Be careful using this method\n"
"since it can result in loading every bound object in the sub-tree.\n"
"This method traverses both map and list containers - for example, a list\n"
"whose elements are lists or maps.\n"
"Returns an iterator over every Object in the sub-tree.";

static PyObject* mod_iter_tree(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    NodelTreeIter* nit = (NodelTreeIter*)NodelTreeIterType.tp_alloc(&NodelTreeIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelTreeIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = nd_self->obj.iter_tree();
        nit->it = nit->range.begin();
        nit->end = nit->range.end();

        Py_INCREF(nit);
        return (PyObject*)nit;
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto key_method_doc =
"Returns the key/index of the argument within its parent.\n"
"nodel.key(obj) -> any\n"
"Returns nil or the key/index of the argument.";

static PyObject* mod_key(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        return support.to_py(nd_self->obj.key());
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto reset_method_doc = \
"Reset a bound object, clearing it and releasing memory resources.\n"
"nodel.reset(obj) -> None\n"
"If the argument is bound (has data-source), then it's cleared and the data-source\n"
"will re-load on the next data access. Any references to descendants will be\n"
"orphaned, and cannot be re-loaded, themselves.";

static PyObject* mod_reset(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        nd_self->obj.reset();
        Py_RETURN_NONE;
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto reset_key_method_doc = \
"Reset the key of a sparse DataSource, clearing it and releasing memory resources.\n"
"nodel.reset_key(obj, key) -> None\n"
"If the object is bound (has data-source), then the key is cleared and the data-source\n"
"will re-load the key on the next access. Any references to descendants of the value\n"
"of the key will be orphaned, and cannot be re-loaded, themselves.";

static PyObject* mod_reset_key(PyObject* self, PyObject* const* args, Py_ssize_t nargs) {
    if (nargs != 2) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_self = as_nodel_object(args[0]);
    if (nd_self == NULL) return NULL;

    try {
        nd_self->obj.reset_key(support.to_key(args[1]));
        Py_RETURN_NONE;
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto save_method_doc = \
"Commit all updates to bound objects in the subtree whose root is the argument.\n"
"nodel.save(obj) -> None\n"
"Objects that are not bound (do not have a data-source) are ignored.";

static PyObject* mod_save(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        nd_self->obj.save();
        Py_RETURN_NONE;
    } catch (const NodelException& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto is_same_method_doc =
"True, if the first and second arguments are the same Object.\n"
"nodel.is_same(l_obj, r_obj) -> bool\n"
"Since the backing C++ class, Object, is a reference, the Python keyword, 'is', will return\n"
"False even if two instances of the C++ Object class point to the same data.\n Therefore,\n"
"this function should always be used to test that two Nodel Objects are the same Object.\n"
"Returns True if the two Nodel Object instances point to the same data";

static PyObject* mod_is_same(PyObject* mod, PyObject* const* args, Py_ssize_t nargs) {
    if (nargs != 2) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* lhs = as_nodel_object(args[0]);
    if (lhs == NULL) return NULL;

    NodelObject* rhs = as_nodel_object(args[1]);
    if (rhs == NULL) return NULL;

    if (lhs->obj.is(rhs->obj)) Py_RETURN_TRUE; else Py_RETURN_FALSE;
}


static PyMethodDef nodel_methods[] = {
    {"bind",        (PyCFunction)mod_bind,                METH_O,        PyDoc_STR(mod_bind_doc)},
    {"from_json",   (PyCFunction)mod_from_json,           METH_O,        PyDoc_STR(mod_from_json_doc)},
    {"clear",       (PyCFunction)mod_clear,               METH_O,        PyDoc_STR(clear_method_doc)},
    {"root",        (PyCFunction)mod_root,                METH_O,        PyDoc_STR(root_method_doc)},
    {"parent",      (PyCFunction)mod_parent,              METH_O,        PyDoc_STR(parent_method_doc)},
    {"iter_keys",   (PyCFunction)mod_iter_keys,           METH_O,        PyDoc_STR(iter_keys_method_doc)},
    {"iter_values", (PyCFunction)mod_iter_values,         METH_O,        PyDoc_STR(iter_values_method_doc)},
    {"iter_items",  (PyCFunction)mod_iter_items,          METH_O,        PyDoc_STR(iter_items_method_doc)},
    {"iter_tree",   (PyCFunction)mod_iter_tree,           METH_O,        PyDoc_STR(iter_tree_method_doc)},
    {"key",         (PyCFunction)mod_key,                 METH_O,        PyDoc_STR(key_method_doc)},
    {"reset",       (PyCFunction)mod_reset,               METH_O,        PyDoc_STR(reset_method_doc)},
    {"reset_key",   (PyCFunction)mod_reset_key,           METH_FASTCALL, PyDoc_STR(reset_key_method_doc)},
    {"save",        (PyCFunction)mod_save,                METH_O,        PyDoc_STR(save_method_doc)},
    {"is_same",     (PyCFunction)mod_is_same,             METH_FASTCALL, PyDoc_STR(is_same_method_doc)},
    {NULL, NULL, 0, NULL}
};


//-----------------------------------------------------------------------------
// Module definition
//-----------------------------------------------------------------------------

PyDoc_STRVAR(module_doc, "Nodel Python Extension");

PyModuleDef nodel_module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "nodel",
    .m_doc = module_doc,
    .m_size = -1,
    .m_methods = nodel_methods,
    .m_slots = NULL,
    .m_traverse = NULL,
    .m_clear = NULL,
    .m_free = NULL
};

PyMODINIT_FUNC
PyInit_nodel(void)
{
    nodel::filesystem::configure();

#ifdef PYNODEL_ROCKSDB
    nodel::kvdb::configure();
#endif

    if (PyType_Ready(&NodelObjectType) < 0) return NULL;
    if (PyType_Ready(&NodelKeyIterType) < 0) return NULL;
    if (PyType_Ready(&NodelValueIterType) < 0) return NULL;
    if (PyType_Ready(&NodelItemIterType) < 0) return NULL;
    if (PyType_Ready(&NodelTreeIterType) < 0) return NULL;

    PyObject* module = PyModule_Create(&nodel_module_def);
    if (module == NULL) return NULL;

    Py_INCREF(&NodelObjectType);
    if (PyModule_AddObject(module, "Object", (PyObject*)&NodelObjectType) < 0) {
        Py_DECREF(&NodelObjectType);
        Py_DECREF(module);
        return NULL;
    }

//    Py_INCREF(&NodelKeyIterType);
//    if (PyModule_AddObject(module, "_KeyIter", (PyObject*)&NodelKeyIterType) < 0) {
//        Py_DECREF(&NodelKeyIterType);
//        Py_DECREF(module);
//        return NULL;
//    }
//
//    Py_INCREF(&NodelValueIterType);
//    if (PyModule_AddObject(module, "_ValueIter", (PyObject*)&NodelValueIterType) < 0) {
//        Py_DECREF(&NodelValueIterType);
//        Py_DECREF(module);
//        return NULL;
//    }
//
//    Py_INCREF(&NodelItemIterType);
//    if (PyModule_AddObject(module, "_ItemIter", (PyObject*)&NodelItemIterType) < 0) {
//        Py_DECREF(&NodelItemIterType);
//        Py_DECREF(module);
//        return NULL;
//    }
//
//    Py_INCREF(&NodelTreeIterType);
//    if (PyModule_AddObject(module, "_TreeIter", (PyObject*)&NodelTreeIterType) < 0) {
//        Py_DECREF(&NodelTreeIterType);
//        Py_DECREF(module);
//        return NULL;
//    }

    nodel_sentinel = PyObject_CallMethodOneArg(module, PyUnicode_InternFromString("Object"), PyUnicode_InternFromString("sentinel"));

    return module;
}

} // extern C

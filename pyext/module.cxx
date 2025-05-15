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
    if (NodelObject_CheckExact(arg)) return (NodelObject*)arg;
    PyErr_SetString(PyExc_ValueError, "Expected a NodelObject");
    return NULL;
}

PyObject* iter_keys(PyObject* obj, PyObject* slice) {
    NodelObject* nd_self = as_nodel_object(obj);
    if (nd_self == NULL) return NULL;

    NodelKeyIter* nit = (NodelKeyIter*)NodelKeyIterType.tp_alloc(&NodelKeyIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelKeyIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = (slice == NULL)? nd_self->obj.iter_keys(): nd_self->obj.iter_keys(support.to_slice(slice));
        nit->it = nit->range.begin();
        nit->end = nit->range.end();

        Py_INCREF(nit);
        return (PyObject*)nit;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

PyObject* iter_values(PyObject* obj, PyObject* slice) {
    NodelObject* nd_self = as_nodel_object(obj);
    if (nd_self == NULL) return NULL;

    NodelValueIter* nit = (NodelValueIter*)NodelValueIterType.tp_alloc(&NodelValueIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelValueIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = (slice == NULL)? nd_self->obj.iter_values(): nd_self->obj.iter_values(support.to_slice(slice));
        nit->it = nit->range.begin();
        nit->end = nit->range.end();

        Py_INCREF(nit);
        return (PyObject*)nit;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

//-----------------------------------------------------------------------------
// Module methods
//-----------------------------------------------------------------------------

constexpr auto new_doc =
"Create a new Nodel Object from the argument.\n"
"The argument must be a string, list or dict.\n"
"new(arg) -> Object\n"
"arg - A Python string, list or dict.";

static PyObject* mod_new(PyObject* mod, PyObject* arg) {
    if (!PyUnicode_Check(arg) && !PyList_Check(arg) && !PyDict_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Expected string, list or dict type");
        return NULL;
    }

    try {
        return (PyObject*)wrap(mod, support.to_object(arg));
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto list_doc =
"Create a new Nodel list Object.\n"
"list() -> Object\n";

static PyObject* mod_list(PyObject* mod) {
    try {
        return (PyObject*)wrap(mod, Object{Object::LIST});
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto dict_doc =
"Create a new Nodel insertion ordered map Object - same as omap().\n"
"dict() -> Object\n";


constexpr auto omap_doc =
"Create a new Nodel insertion ordered map Object.\n"
"omap() -> Object\n";

static PyObject* mod_omap(PyObject* mod) {
    try {
        return (PyObject*)wrap(mod, Object{Object::OMAP});
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto smap_doc =
"Create a new Nodel sorted map Object.\n"
"smap() -> Object\n";

static PyObject* mod_smap(PyObject* mod) {
    try {
        return (PyObject*)wrap(mod, Object{Object::SMAP});
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto bind_doc =
"Bind object with URI string/dict\n"
"bind(uri) -> Object\n"
"uri - A URI string with a registered scheme";

static PyObject* mod_bind(PyObject* mod, PyObject* arg) {
    try {
        URI uri{support.to_object(arg)};
        return (PyObject*)wrap(mod, bind(uri));
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto from_json_doc =
"Parse JSON into an Object\n"
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

constexpr auto clear_doc =
"Removes all elements from a container.\n"
"nodel.clear(obj) -> None\n"
"This method does nothing if the object is not a container.";

static PyObject* mod_clear(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    try {
        nd_self->obj.clear();
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
    Py_RETURN_NONE;
}

constexpr auto del_from_parent_doc =
"Removes this object from its parent container.\n"
"nodel.clear(obj) -> None\n"
"This method does nothing if the object does not have a parent.";

static PyObject* mod_del_from_parent(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    try {
        nd_self->obj.del_from_parent();
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
    Py_RETURN_NONE;
}

constexpr auto root_doc =
"Returns the root of the tree containing the argument.\n"
"nodel.root(obj) -> Object\n"
"Returns the root of the tree, which may be the argument.";

static PyObject* mod_root(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    try {
        return (PyObject*)wrap(mod, nd_self->obj.root());
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto parent_doc =
"Returns the parent of the argument.\n"
"nodel.parent(obj) -> Object\n"
"If the argument is the root of the tree, then nil is returned.\n"
"Returns the parent of the argument, which may be nil.";

static PyObject* mod_parent(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    try {
        return (PyObject*)wrap(mod, nd_self->obj.parent());
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto iter_keys_doc =
"Returns an iterator over the keys of the first argument.\n"
"The optional second argument is a Python slice object.\n"
"nodel.iter_keys(obj, slice=None) -> iterator\n"
"If the argument is any kind of map, then this method returns an iterator over\n"
"keys in the map.\n"
"If the argument is a list, then this method returns an iterator over the\n"
"indices of the list - similar to the `enumerate` builtin function.\n"
"Both maps and lists support the slice argument.\n"
"Otherwise, a RuntimeError exception is raised.\n"
"Returns an iterator over the keys of the argument.";

static PyObject* mod_iter_keys(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs < 1 || nargs > 2) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    try {
        return iter_keys(args[0], (nargs == 1)? NULL: args[1]);
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto iter_values_doc =
"Returns an iterator over the values (Object instances) of the argument.\n"
"The optional second argument is a Python slice object.\n"
"nodel.iter_values(obj, slice=None) -> iterator\n"
"If the argument is any kind of map, then this method returns an iterator over\n"
"values in the map.\n"
"If the argument is a list, then this method returns an iterator over the\n"
"elements in the list.\n"
"Both maps and lists support the slice argument.\n"
"Otherwise, a RuntimeError exception is raised.\n"
"Returns an iterator over the values of the argument.";

static PyObject* mod_iter_values(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs < 1 || nargs > 2) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_self = as_nodel_object(args[0]);
    if (nd_self == NULL) return NULL;

    try {
        return iter_values(args[0], (nargs == 1)? NULL: args[1]);
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto iter_items_doc =
"Returns an iterator over the items of the argument.\n"
"The optional second argument is a Python slice object.\n"
"nodel.iter_items(obj, slice=None) -> iterator\n"
"If the argument is any kind of map, then this method returns an iterator over\n"
"key/value pairs in the map.\n"
"If the argument is a list, then this method returns an iterator over the\n"
"index/value pairs in the list, similar to the `enumerate` builtin function.\n"
"Both maps and lists support the slice argument.\n"
"Otherwise, a RuntimeError exception is raised.\n"
"Returns an iterator over the items of the argument.";

static PyObject* mod_iter_items(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs < 1 || nargs > 2) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_self = as_nodel_object(args[0]);
    if (nd_self == NULL) return NULL;

    NodelItemIter* nit = (NodelItemIter*)NodelItemIterType.tp_alloc(&NodelItemIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelItemIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = (nargs == 1)? nd_self->obj.iter_items(): nd_self->obj.iter_items(support.to_slice(args[1]));
        nit->it = nit->range.begin();
        nit->end = nit->range.end();

        Py_INCREF(nit);
        return (PyObject*)nit;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto iter_tree_doc =
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
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto key_doc =
"Returns the key/index of the argument within its parent.\n"
"nodel.key(obj) -> any\n"
"Returns nil or the key/index of the argument.";

static PyObject* mod_key(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        return support.to_py(nd_self->obj.key());
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto get_doc =
"Calls Object::get with semantics like Python dict.get.\n"
"Note that this function works for both map and list Objects.\n"
"nodel.get(key, default=None) -> any\n"
"Returns the value of key/index, or the default value.";

static PyObject* mod_get(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs < 2 || nargs > 3) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    try {
        auto key = support.to_key(args[1]);
        auto value = nd_obj->obj.get(key);
        if (value.is_nil())
            value = (nargs < 3)? nil: support.to_object(args[2]);
        return support.prepare_return_value(value);
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto reset_doc = \
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
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto reset_key_doc = \
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
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto save_doc = \
"Commit all updates to bound objects in the subtree whose root is the argument.\n"
"nodel.save(obj) -> None\n"
"Objects that are not bound (do not have a data-source) are ignored.";

static PyObject* mod_save(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        nd_self->obj.save();
        Py_RETURN_NONE;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto fpath_doc = \
"Returns the filesystem path of a bound Nodel Object or None.\n"
"nodel.fpath(obj) -> str\n";

static PyObject* mod_fpath(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        auto path = nodel::filesystem::path(nd_self->obj);
        return python::to_str(path.string());
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto is_bound_doc =
"Returns True if the argument is a Nodel Object that has a data-source.";

static PyObject* mod_is_bound(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    try {
        if (nodel::has_data_source(nd_self->obj)) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto is_same_doc =
"True, if the first and second arguments are the same Object.\n"
"nodel.is_same(l_obj, r_obj) -> bool\n"
"Since the backing C++ class, Object, is a reference, the Python keyword, 'is', will return\n"
"False even if two instances of the C++ Object class point to the same data.\n Therefore,\n"
"this function should always be used to test that two Nodel Objects are the same Object.\n"
"Returns True if the two Nodel Object instances point to the same data.";

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

static PyObject* is_type(PyObject* mod, PyObject* arg, Object::ReprIX repr_ix) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        if (nd_self->obj.type() == repr_ix) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto is_nil_doc = \
"True, if the type of the object is NIL.\n"
"Although NIL is similar to Python None, since all types are represented by the C++\n"
"nodel::Object classm, a simple test using 'is None' will not work.";

static PyObject* mod_is_nil(PyObject* mod, PyObject* arg) { return is_type(mod, arg, Object::ReprIX::NIL); }

constexpr auto is_bool_doc = "True, if the type of the object is a boolean.\n";
static PyObject* mod_is_bool(PyObject* mod, PyObject* arg) { return is_type(mod, arg, Object::ReprIX::BOOL); }

constexpr auto is_int_doc = "True, if the type of the object is a signed or unsigned integer.\n";

static PyObject* mod_is_int(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        if (nd_self->obj.type() == Object::ReprIX::INT || nd_self->obj.type() == Object::ReprIX::UINT) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto is_float_doc = "True, if the type of the object is a float.\n";
static PyObject* mod_is_float(PyObject* mod, PyObject* arg) { return is_type(mod, arg, Object::ReprIX::FLOAT); }

constexpr auto is_list_doc = "True, if the type of the object is a list.\n";
static PyObject* mod_is_list(PyObject* mod, PyObject* arg) { return is_type(mod, arg, Object::ReprIX::LIST); }

constexpr auto is_str_doc = "True, if the type of the object is a string.\n";
static PyObject* mod_is_str(PyObject* mod, PyObject* arg) { return is_type(mod, arg, Object::ReprIX::STR); }

constexpr auto is_map_doc = "True, if the type of the object is any type of map.\n";

static PyObject* mod_is_map(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;

    try {
        if (nd_self->obj.type() == Object::ReprIX::OMAP || nd_self->obj.type() == Object::ReprIX::SMAP) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto is_native_doc = "True, if the object is an alien wrapper for a native Python object.\n";
static PyObject* mod_is_native(PyObject* mod, PyObject* arg) { return is_type(mod, arg, Object::ReprIX::ANY); }

constexpr auto native_doc = \
"Convert the object into a native python type.\n"
"To avoid deep copies, container types are not supported.\n"
"Currently, map types are not supported.\n";

static PyObject* mod_native(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    return support.to_py(nd_self->obj);
}

constexpr auto ref_count_doc = \
"Returns the reference count of a Nodel Object.\n"
"Note that this reference count represents the count of references to the underlying\n"
"nodel::Object instance, which is independent of the Python reference count for the\n"
"associated Python object.\n";

static PyObject* mod_ref_count(PyObject* mod, PyObject* arg) {
    NodelObject* nd_self = as_nodel_object(arg);
    if (nd_self == NULL) return NULL;
    return PyLong_FromUnsignedLongLong(nd_self->obj.ref_count());
}

static PyMethodDef nodel_methods[] = {
    {"new",         (PyCFunction)mod_new,                 METH_O,        PyDoc_STR(new_doc)},
    {"list",        (PyCFunction)mod_list,                METH_NOARGS,   PyDoc_STR(list_doc)},
    {"dict",        (PyCFunction)mod_omap,                METH_NOARGS,   PyDoc_STR(dict_doc)},
    {"omap",        (PyCFunction)mod_omap,                METH_NOARGS,   PyDoc_STR(omap_doc)},
    {"smap",        (PyCFunction)mod_smap,                METH_NOARGS,   PyDoc_STR(smap_doc)},
    {"bind",        (PyCFunction)mod_bind,                METH_O,        PyDoc_STR(bind_doc)},
    {"from_json",   (PyCFunction)mod_from_json,           METH_O,        PyDoc_STR(from_json_doc)},
    {"clear",       (PyCFunction)mod_clear,               METH_O,        PyDoc_STR(clear_doc)},
    {"root",        (PyCFunction)mod_root,                METH_O,        PyDoc_STR(root_doc)},
    {"parent",      (PyCFunction)mod_parent,              METH_O,        PyDoc_STR(parent_doc)},
    {"iter_keys",   (PyCFunction)mod_iter_keys,           METH_FASTCALL, PyDoc_STR(iter_keys_doc)},
    {"iter_values", (PyCFunction)mod_iter_values,         METH_FASTCALL, PyDoc_STR(iter_values_doc)},
    {"iter_items",  (PyCFunction)mod_iter_items,          METH_FASTCALL, PyDoc_STR(iter_items_doc)},
    {"iter_tree",   (PyCFunction)mod_iter_tree,           METH_O,        PyDoc_STR(iter_tree_doc)},
    {"key",         (PyCFunction)mod_key,                 METH_O,        PyDoc_STR(key_doc)},
    {"get",         (PyCFunction)mod_get,                 METH_FASTCALL, PyDoc_STR(get_doc)},
    {"reset",       (PyCFunction)mod_reset,               METH_O,        PyDoc_STR(reset_doc)},
    {"reset_key",   (PyCFunction)mod_reset_key,           METH_FASTCALL, PyDoc_STR(reset_key_doc)},
    {"save",        (PyCFunction)mod_save,                METH_O,        PyDoc_STR(save_doc)},
    {"fpath",       (PyCFunction)mod_fpath,               METH_O,        PyDoc_STR(fpath_doc)},
    {"is_bound",    (PyCFunction)mod_is_bound,            METH_O,        PyDoc_STR(is_bound_doc)},
    {"is_same",     (PyCFunction)mod_is_same,             METH_FASTCALL, PyDoc_STR(is_same_doc)},
    {"is_nil",      (PyCFunction)mod_is_nil,              METH_O,        PyDoc_STR(is_nil_doc)},
    {"is_bool",     (PyCFunction)mod_is_bool,             METH_O,        PyDoc_STR(is_bool_doc)},
    {"is_int",      (PyCFunction)mod_is_int,              METH_O,        PyDoc_STR(is_int_doc)},
    {"is_float",    (PyCFunction)mod_is_float,            METH_O,        PyDoc_STR(is_float_doc)},
    {"is_str",      (PyCFunction)mod_is_str,              METH_O,        PyDoc_STR(is_str_doc)},
    {"is_list",     (PyCFunction)mod_is_list,             METH_O,        PyDoc_STR(is_list_doc)},
    {"is_map",      (PyCFunction)mod_is_map,              METH_O,        PyDoc_STR(is_map_doc)},
    {"is_native",   (PyCFunction)mod_is_native,           METH_O,        PyDoc_STR(is_native_doc)},
    {"native",      (PyCFunction)mod_native,              METH_O,        PyDoc_STR(native_doc)},
    {"ref_count",   (PyCFunction)mod_ref_count,           METH_O,        PyDoc_STR(ref_count_doc)},
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

    nodel_sentinel = PyObject_CallMethodOneArg(module, PyUnicode_InternFromString("Object"), PyUnicode_InternFromString("sentinel"));

    return module;
}

} // extern C

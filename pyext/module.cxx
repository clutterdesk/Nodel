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

static Object convert_kwargs_to_object(PyObject *const *args, Py_ssize_t nargs, PyObject* kwnames) {
    Object obj{Object::OMAP};

    Py_ssize_t nkwargs = PyTuple_GET_SIZE(kwnames);

    try {
        for (Py_ssize_t i = 0; i < nkwargs; ++i) {
            PyObject* py_key = PyTuple_GET_ITEM(kwnames, i);
            PyObject* py_value = args[nargs + i];
            obj.set(support.to_key(py_key), support.to_object(py_value));
        }
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return nil;
    }

    return obj;
}

PyObject* iter_keys(PyObject* obj, PyObject* slice) {
    NodelObject* nd_obj = as_nodel_object(obj);
    if (nd_obj == NULL) return NULL;

    NodelKeyIter* nit = (NodelKeyIter*)NodelKeyIterType.tp_alloc(&NodelKeyIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelKeyIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = (slice == NULL)? nd_obj->obj.iter_keys(): nd_obj->obj.iter_keys(support.to_slice(slice));
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
    NodelObject* nd_obj = as_nodel_object(obj);
    if (nd_obj == NULL) return NULL;

    NodelValueIter* nit = (NodelValueIter*)NodelValueIterType.tp_alloc(&NodelValueIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelValueIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = (slice == NULL)? nd_obj->obj.iter_values(): nd_obj->obj.iter_values(support.to_slice(slice));
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
    if (PyBool_Check(arg) || PyLong_Check(arg) || PyFloat_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Invalid type");
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

constexpr auto insert_doc =
"Insert an obj2 into the container, obj1, at the specified key.\n"
"If the container is a list then the key must be an integer.\n"
"If the container is a map then this function is equivalent to 'obj1[key] = obj2'.\n"
"insert(obj1, key, obj2) -> Object\n"
"Returns the object that was actually inserted, which will be a clone if the object\n"
"    already resides in a different container (already has a parent).\n";

static PyObject* mod_insert(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs != 3) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    auto result = nd_obj->obj.insert(support.to_key(args[1]), support.to_object(args[2]));
    return (PyObject*)wrap(mod, result);
}

constexpr auto append_doc =
"Append the argument to a list.\n"
"append(obj1, obj2) -> Object\n"
"Returns the object that was actually append, which will be a clone if the object\n"
"    already resides in a different container (already has a parent).\n";

static PyObject* mod_append(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs != 2) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    auto result = nd_obj->obj.append(support.to_object(args[1]));
    return (PyObject*)wrap(mod, result);
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
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;
    try {
        nd_obj->obj.clear();
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
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;
    try {
        nd_obj->obj.del_from_parent();
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
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;
    try {
        return (PyObject*)wrap(mod, nd_obj->obj.root());
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
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;
    try {
        auto parent = nd_obj->obj.parent();
        if (parent.is_nil()) {
            Py_RETURN_NONE;
        }
        return (PyObject*)wrap(mod, parent);
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

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

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

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    NodelItemIter* nit = (NodelItemIter*)NodelItemIterType.tp_alloc(&NodelItemIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelItemIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = (nargs == 1)? nd_obj->obj.iter_items(): nd_obj->obj.iter_items(support.to_slice(args[1]));
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
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;

    NodelTreeIter* nit = (NodelTreeIter*)NodelTreeIterType.tp_alloc(&NodelTreeIterType, 0);
    if (nit == NULL) return NULL;
    if (NodelTreeIterType.tp_init((PyObject*)nit, NULL, NULL) == -1) return NULL;

    try {
        nit->range = nd_obj->obj.iter_tree();
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
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;

    try {
        return support.to_py(nd_obj->obj.key());
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto path_doc =
"Returns the path of the object from the root of the datamodel.  The return type\n"
"can be either a string (mode='str') containing a valid python expression, or a\n"
"list (mode='list') of keys.  As a string the return type could be passed to the\n"
"python 'eval' function to be executed relative to a nodel Object.\n"
"nodel.path(obj, mode) -> str\n"
"Returns the path of the argument.";

static PyObject* mod_path(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs < 1 || nargs > 2) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    try {
        bool as_list = false;
        if (nargs == 2) {
            if (PyUnicode_Check(args[1])) {
                as_list = PyUnicode_EqualToUTF8AndSize(args[1], "list", 4) == 1;
            } else {
                PyErr_SetString(PyExc_TypeError, "Expected str for argument 2");
                return NULL;
            }
        }
        if (as_list) {
            return support.to_py(nd_obj->obj.path());
        } else {
            return python::to_str(nd_obj->obj.path().to_str());
        }
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

static PyObject* get_impl(NodelObject* nd_obj, PyObject* key, PyObject* default_value) {
    try {
        Object value;

        if (PyUnicode_Check(key)) {
            value = nd_obj->obj.get(OPath{python::to_string_view(key)});
        } else if (PyLong_Check(key)) {
            value = nd_obj->obj.get(support.to_key(key));
        } else if (NodelObject_CheckExact(key)) {
            auto arg = as_nodel_object(key);
            Object value = nd_obj->obj;
            for (const auto& obj : arg->obj.iter_values()) {
                value = value.get(obj.to_key());
                if (value.is_nil()) break;
            }
        } else if (PyList_Check(key)) {
            value = nd_obj->obj;
            Py_ssize_t size = PyList_GET_SIZE(key);
            for (Py_ssize_t i=0; i<size; i++) {
                PyObject* py_key = PyList_GET_ITEM(key, i);  // borrowed reference
                auto key = support.to_key(py_key);
                if (PyErr_Occurred()) return NULL;
                value = value.get(key);
                if (value.is_nil()) break;
            }
        } else if (PyTuple_Check(key)) {
            value = nd_obj->obj;
            Py_ssize_t size = PyTuple_GET_SIZE(key);
            for (Py_ssize_t i=0; i<size; i++) {
                PyObject* py_key = PyTuple_GET_ITEM(key, i);  // borrowed reference
                auto key = support.to_key(py_key);
                if (PyErr_Occurred()) return NULL;
                value = value.get(key);
                if (value.is_nil()) break;
            }
        } else {
            PyErr_SetString(PyExc_ValueError, "Expected a NodelObject");
            return NULL;
        }

        if (value.is_nil()) {
            if (default_value != NULL) {
                return support.prepare_return_value(support.to_object(default_value));
            } else {
                Py_RETURN_NONE;
            }
        }

        return support.prepare_return_value(value);
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

    return get_impl(nd_obj, args[1], (nargs == 3)? args[2]: NULL);
}


constexpr auto get_set_doc = \
"Similar to nodel::get function, but the default value is inserted at the index if\n"
"the key does not exist.  The default argument may be a Python type, in which case\n"
"an instance of the type will be constructed with the remainin arguments, if needed.\n"
"nodel.get_set(key, default, ...) -> any\n"
"Returns the value of key/index, or the default value.";

static PyObject* mod_get_set(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs < 3) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    auto value = get_impl(nd_obj, args[1], NULL);
    if (nargs == 2) return value;

    if (value == Py_None) {
        Py_DECREF(value);

        if (PyType_Check(args[2])) {
            RefMgr new_args = PyTuple_New(nargs - 3);
            for (Py_ssize_t i=3; i<nargs; i++) {
                Py_INCREF(args[i]);
                PyTuple_SET_ITEM(new_args.get(), i - 3, args[i]);
            }
            RefMgr po = PyObject_CallObject(args[2], new_args.get());
            if (po == NULL) return NULL;
            value = mod_new(mod, po.get());
        } else {
            value = mod_new(mod, args[2]);
        }

        if (value == NULL) return NULL;
        nd_obj->obj.set(support.to_key(args[1]), ((NodelObject*)value)->obj);
    }

    return value;
}

constexpr auto complete_doc = \
"Create objects necessary to complete this path. The object argument must be a list\n"
"or map. The path is a list of keys or a string of the form returned by the path\n"
"function. The default argument can be either a type or an object.  If it is a type\n"
"then the remaining arguments are used to instantiate the type.\n"
"node.complete(obj, path, default, ...) -> any\n"
"Returns the leaf of the path.";

static PyObject* mod_complete(PyObject* mod, PyObject *const *args, Py_ssize_t nargs) {
    if (nargs < 3) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    try {
        Object value;
        OPath path;

        if (PyUnicode_Check(args[1])) {
            path = OPath{python::to_string_view(args[1])};
            value = nd_obj->obj.get(path);
        } else if (PyList_Check(args[1])) {
            auto keys = support.to_key_list(args[1]);
            if (!keys.has_value()) return NULL;
            path = OPath{keys.value()};
            value = nd_obj->obj.get(path);
        } else {
            PyErr_SetString(PyExc_ValueError, "Expected str or list");
            return NULL;
        }

        if (value.is_nil()) {
            Object default_value;
            if (PyType_Check(args[2])) {
                RefMgr new_args = PyTuple_New(nargs - 3);
                for (Py_ssize_t i=3; i<nargs; i++) {
                    Py_INCREF(args[i]);
                    PyTuple_SET_ITEM(new_args.get(), i - 3, args[i]);
                }
                RefMgr po = PyObject_CallObject(args[2], new_args.get());
                if (po == NULL) return NULL;
                default_value = support.to_object(po.get());
            } else {
                default_value = support.to_object(args[2]);
            }

            value = path.create(nd_obj->obj, default_value);
        }

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
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;

    try {
        nd_obj->obj.reset();
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

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    try {
        nd_obj->obj.reset_key(support.to_key(args[1]));
        Py_RETURN_NONE;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto save_doc = \
"Commit all updates to bound objects in the subtree whose root is the argument.\n"
"Data-source-specific options can be passed as keywords, for example, indent=2.\n"
"nodel.save(obj, **options) -> None\n"
"Objects that are not bound (do not have a data-source) are ignored.";

static PyObject* mod_save(PyObject* mod, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames) {
    Py_ssize_t n_kwargs = (kwnames != NULL)? PyTuple_GET_SIZE(kwnames): 0;

    if (nargs != 1) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    try {
        if (n_kwargs > 0) {
            nd_obj->obj.save(convert_kwargs_to_object(args, nargs, kwnames));
        } else {
            nd_obj->obj.save();
        }
        Py_RETURN_NONE;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto is_unsaved_doc = \
"Returns True if the argument has a data-source and has uncommitted updates.\n"
"nodel.is_unsaved() -> bool\n";

static PyObject* mod_is_unsaved(PyObject* mod, PyObject* arg) {
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;

    try {
        if (nd_obj->obj.is_unsaved()) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto set_unsaved_doc = \
"Marks or clears the stale flag for an object with a data-source.\n"
"The stale flag controls whether the object will be stored on the next call to the save method.\n"
"nodel.set_unsaved(bool)\n";

static PyObject* mod_set_unsaved(PyObject* mod, PyObject* const* args, Py_ssize_t nargs) {
    if (nargs != 2) {
        PyErr_SetString(PyExc_ValueError, "Wrong number of arguments");
        return NULL;
    }

    NodelObject* nd_obj = as_nodel_object(args[0]);
    if (nd_obj == NULL) return NULL;

    try {
        nd_obj->obj.set_unsaved(PyObject_IsTrue(args[1]));
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
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;

    try {
        auto path = nodel::filesystem::path(nd_obj->obj);
        return python::to_str(path.string());
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

constexpr auto id_doc = \
"Returns the unique ID of an Object.";

static PyObject* mod_id(PyObject* mod, PyObject* arg) {
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;
    return python::to_str(nd_obj->obj.id().to_str());
}

constexpr auto is_bound_doc =
"Returns True if the argument is a Nodel Object that has a data-source.";

static PyObject* mod_is_bound(PyObject* mod, PyObject* arg) {
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;
    try {
        if (nodel::has_data_source(nd_obj->obj)) Py_RETURN_TRUE;
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
    if (lhs == NULL) {
        PyErr_Clear();
        if (args[0] == args[1]) Py_RETURN_TRUE; else Py_RETURN_FALSE;
    }

    NodelObject* rhs = as_nodel_object(args[1]);
    if (rhs == NULL) {
        PyErr_Clear();
        Py_RETURN_FALSE;
    }

    if (lhs->obj.is(rhs->obj)) Py_RETURN_TRUE; else Py_RETURN_FALSE;
}

static PyObject* is_type(PyObject* mod, PyObject* arg, Object::ReprIX repr_ix) {
    if (!NodelObject_CheckExact(arg)) {
        Py_RETURN_FALSE;
    } else {
        NodelObject* nd_obj = (NodelObject*)arg;
        try {
            if (nd_obj->obj.type() == repr_ix) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        } catch (const std::exception& ex) {
            PyErr_SetString(PyExc_RuntimeError, ex.what());
            return NULL;
        }
    }
}

constexpr auto is_str_doc =
"True, if argument is a nodel string Object.\n";

static PyObject* mod_is_str(PyObject* mod, PyObject* arg) {
    if (!NodelObject_CheckExact(arg)) {
        Py_RETURN_FALSE;
    } else {
        return is_type(mod, arg, Object::ReprIX::STR);
    }
}

constexpr auto is_list_doc =
"True, if argument is a nodel list Object.\n";

static PyObject* mod_is_list(PyObject* mod, PyObject* arg) {
    if (!NodelObject_CheckExact(arg)) {
        Py_RETURN_FALSE;
    } else {
        return is_type(mod, arg, Object::ReprIX::LIST);
    }
}

constexpr auto is_map_doc =
"True, if argument is a nodel map Object.\n";

static PyObject* mod_is_map(PyObject* mod, PyObject* arg) {
    if (!NodelObject_CheckExact(arg)) {
        Py_RETURN_FALSE;
    } else {
        NodelObject* nd_obj = (NodelObject*)arg;
        try {
            if (nd_obj->obj.type() == Object::ReprIX::OMAP || nd_obj->obj.type() == Object::ReprIX::SMAP) Py_RETURN_TRUE;
            Py_RETURN_FALSE;
        } catch (const std::exception& ex) {
            PyErr_SetString(PyExc_RuntimeError, ex.what());
            return NULL;
        }
    }
}

constexpr auto is_str_like_doc =
"True, if object is a python string object or a nodel string Object.";

static PyObject* mod_is_str_like(PyObject* mod, PyObject* arg) {
    if (PyUnicode_Check(arg)) {
        Py_RETURN_TRUE;
    } else {
        return is_type(mod, arg, Object::ReprIX::STR);
    }
}

constexpr auto is_list_like_doc =
"True, if object is a python list object or a nodel list Object.";

static PyObject* mod_is_list_like(PyObject* mod, PyObject* arg) {
    if (PyList_Check(arg)) {
        Py_RETURN_TRUE;
    } else {
        return is_type(mod, arg, Object::ReprIX::LIST);
    }
}

constexpr auto is_map_like_doc =
"True, if object is a python map object or a nodel map Object.";

static PyObject* mod_is_map_like(PyObject* mod, PyObject* arg) {
    if (PyDict_Check(arg)) {
        Py_RETURN_TRUE;
    } else {
        return mod_is_map(mod, arg);
    }
}

constexpr auto is_alien_doc = "True, if the object is an alien wrapper for a native Python object.\n";
static PyObject* mod_is_alien(PyObject* mod, PyObject* arg) { return is_type(mod, arg, Object::ReprIX::ANY); }

constexpr auto native_doc = \
"Convert the object into a native python type.\n"
"To avoid deep copies, container types are not supported.\n"
"Currently, map types are not supported.\n";

static PyObject* mod_native(PyObject* mod, PyObject* arg) {
    if (NodelObject_CheckExact(arg)) {
        NodelObject* nd_obj = as_nodel_object(arg);
        if (nd_obj == NULL) return NULL;
        return support.to_py(nd_obj->obj);
    } else {
        Py_INCREF(arg);
        return arg;
    }
}

constexpr auto ref_count_doc = \
"Returns the reference count of a Nodel Object.\n"
"Note that this reference count represents the count of references to the underlying\n"
"nodel::Object instance, which is independent of the Python reference count for the\n"
"associated Python object.\n";

static PyObject* mod_ref_count(PyObject* mod, PyObject* arg) {
    NodelObject* nd_obj = as_nodel_object(arg);
    if (nd_obj == NULL) return NULL;
    return PyLong_FromUnsignedLongLong(nd_obj->obj.ref_count());
}

static PyMethodDef nodel_methods[] = {
    {"new",          (PyCFunction)mod_new,                 METH_O,        PyDoc_STR(new_doc)},
    {"list",         (PyCFunction)mod_list,                METH_NOARGS,   PyDoc_STR(list_doc)},
    {"dict",         (PyCFunction)mod_omap,                METH_NOARGS,   PyDoc_STR(dict_doc)},
    {"omap",         (PyCFunction)mod_omap,                METH_NOARGS,   PyDoc_STR(omap_doc)},
    {"smap",         (PyCFunction)mod_smap,                METH_NOARGS,   PyDoc_STR(smap_doc)},
    {"append",       (PyCFunction)mod_append,              METH_FASTCALL, PyDoc_STR(append_doc)},
    {"insert",       (PyCFunction)mod_insert,              METH_FASTCALL, PyDoc_STR(insert_doc)},
    {"bind",         (PyCFunction)mod_bind,                METH_O,        PyDoc_STR(bind_doc)},
    {"from_json",    (PyCFunction)mod_from_json,           METH_O,        PyDoc_STR(from_json_doc)},
    {"clear",        (PyCFunction)mod_clear,               METH_O,        PyDoc_STR(clear_doc)},
    {"root",         (PyCFunction)mod_root,                METH_O,        PyDoc_STR(root_doc)},
    {"parent",       (PyCFunction)mod_parent,              METH_O,        PyDoc_STR(parent_doc)},
    {"iter_keys",    (PyCFunction)mod_iter_keys,           METH_FASTCALL, PyDoc_STR(iter_keys_doc)},
    {"iter_values",  (PyCFunction)mod_iter_values,         METH_FASTCALL, PyDoc_STR(iter_values_doc)},
    {"iter_items",   (PyCFunction)mod_iter_items,          METH_FASTCALL, PyDoc_STR(iter_items_doc)},
    {"iter_tree",    (PyCFunction)mod_iter_tree,           METH_O,        PyDoc_STR(iter_tree_doc)},
    {"key",          (PyCFunction)mod_key,                 METH_O,        PyDoc_STR(key_doc)},
    {"path",         (PyCFunction)mod_path,                METH_FASTCALL, PyDoc_STR(path_doc)},
    {"get",          (PyCFunction)mod_get,                 METH_FASTCALL, PyDoc_STR(get_doc)},
    {"get_set",      (PyCFunction)mod_get_set,             METH_FASTCALL, PyDoc_STR(get_set_doc)},
    {"complete",     (PyCFunction)mod_complete,            METH_FASTCALL, PyDoc_STR(complete_doc)},
    {"reset",        (PyCFunction)mod_reset,               METH_O,        PyDoc_STR(reset_doc)},
    {"reset_key",    (PyCFunction)mod_reset_key,           METH_FASTCALL, PyDoc_STR(reset_key_doc)},
    {"save",         (PyCFunction)mod_save,                METH_FASTCALL |
                                                           METH_KEYWORDS, PyDoc_STR(save_doc)},
    {"is_unsaved",   (PyCFunction)mod_is_unsaved,          METH_O,        PyDoc_STR(is_unsaved_doc)},
    {"set_unsaved",  (PyCFunction)mod_set_unsaved,         METH_FASTCALL, PyDoc_STR(set_unsaved_doc)},
    {"fpath",        (PyCFunction)mod_fpath,               METH_O,        PyDoc_STR(fpath_doc)},
    {"reset",        (PyCFunction)mod_reset,               METH_O,        PyDoc_STR(reset_doc)},
    {"id",           (PyCFunction)mod_id,                  METH_O,        PyDoc_STR(id_doc)},
    {"is_bound",     (PyCFunction)mod_is_bound,            METH_O,        PyDoc_STR(is_bound_doc)},
    {"is_same",      (PyCFunction)mod_is_same,             METH_FASTCALL, PyDoc_STR(is_same_doc)},
    {"is_str",       (PyCFunction)mod_is_str,              METH_O,        PyDoc_STR(is_str_doc)},
    {"is_list",      (PyCFunction)mod_is_list,             METH_O,        PyDoc_STR(is_list_doc)},
    {"is_map",       (PyCFunction)mod_is_map,              METH_O,        PyDoc_STR(is_map_doc)},
    {"is_str_like",  (PyCFunction)mod_is_str_like,         METH_O,        PyDoc_STR(is_str_like_doc)},
    {"is_list_like", (PyCFunction)mod_is_list_like,        METH_O,        PyDoc_STR(is_list_like_doc)},
    {"is_map_like",  (PyCFunction)mod_is_map_like,         METH_O,        PyDoc_STR(is_map_like_doc)},
    {"is_alien",     (PyCFunction)mod_is_alien,            METH_O,        PyDoc_STR(is_alien_doc)},
    {"native",       (PyCFunction)mod_native,              METH_O,        PyDoc_STR(native_doc)},
    {"ref_count",    (PyCFunction)mod_ref_count,           METH_O,        PyDoc_STR(ref_count_doc)},
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

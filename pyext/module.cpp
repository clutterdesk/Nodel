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
#include <nodel/support/intern.h>
#include <nodel/parser/json.h>
#include <nodel/filesystem.h>

NODEL_INTERN_STATIC_INIT;

using namespace nodel;
using RefMgr = python::RefMgr;

extern "C" {

PyObject* nodel_sentinel = nullptr;

//-----------------------------------------------------------------------------
// Module methods
//-----------------------------------------------------------------------------

static PyObject* mod_bind_dir(PyObject* mod, PyObject* arg) {
    auto opt_path = python::to_string_view(arg);
    if (!opt_path) return NULL;
    std::filesystem::path path = *opt_path;
    return (PyObject*)NodelObject_wrap(filesystem::bind(path));
}

static PyObject* mod_from_json(PyObject* mod, PyObject* arg) {
    Py_ssize_t size;
    auto c_str = PyUnicode_AsUTF8AndSize(arg, &size);
    if (c_str == NULL) return NULL;

    RefMgr po = PyObject_CallMethodObjArgs(mod, PyUnicode_InternFromString("Object"), NULL);
    if (po == NULL) return NULL;

    NodelObject* obj = (NodelObject*)po.get();
    std::string_view spec{c_str, (std::string_view::size_type)size};
    std::optional<json::Error> err;
    obj->obj = json::parse(spec, err);

    if (err) {
        std::string msg = SyntaxError::make_message(spec, err->error_offset, err->error_message);
        PyErr_SetString(PyExc_ValueError, msg.data());
        return NULL;
    }

    return po.get_clear();
}

static PyMethodDef NodelMethods[] = {
    {"bind_dir",  (PyCFunction)mod_bind_dir,  METH_O,
            PyDoc_STR("Bind a new nodel.Object to a filesystem directory.")},
    {"from_json", (PyCFunction)mod_from_json, METH_O,
            PyDoc_STR("Parse json and return object.")},
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
    .m_methods = NodelMethods,
    .m_slots = NULL,
    .m_traverse = NULL,
    .m_clear = NULL,
    .m_free = NULL
};

PyMODINIT_FUNC
PyInit_nodel(void)
{
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

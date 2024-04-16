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
#pragma once

#include <Python.h>
#include <nodel/core/Key.h>
#include <nodel/core/Object.h>
#include <nodel/support/logging.h>

namespace nodel {

namespace python {

class RefMgr
{
  public:
    RefMgr()              : m_ref{nullptr} {}
    RefMgr(PyObject* ref) : m_ref{ref}     {}
    ~RefMgr()                              { Py_XDECREF(m_ref); }

    void clear()          { m_ref = nullptr; }
    PyObject* get() const { return m_ref; }
    PyObject* get_clear() { PyObject* ref = get(); clear(); return ref; }
    operator PyObject* () const { return m_ref; }  // implicit cast

  private:
    PyObject* m_ref;
};

inline
std::optional<std::string_view> to_string_view(PyObject* po) {
    RefMgr unicode;
    if (!PyUnicode_Check(po)) {
        unicode = PyObject_Str(po);
    } else {
        unicode = po;
        Py_INCREF(unicode);
    }

    std::optional<std::string_view> result;

    Py_ssize_t size;
    const char* str = PyUnicode_AsUTF8AndSize(unicode, &size);
    if (str != nullptr) {
        result = {str, (std::string_view::size_type)size};
    }

    return result;
}

inline
PyObject* to_str(int32_t value) {
    auto py_int = PyLong_FromLong(value);
    if (py_int == NULL) return NULL;
    return PyObject_Str(py_int);
}

inline
PyObject* to_str(int64_t value) {
    auto py_int = PyLong_FromLongLong(value);
    if (py_int == NULL) return NULL;
    return PyObject_Str(py_int);
}

inline
PyObject* to_str(uint32_t value) {
    auto py_int = PyLong_FromUnsignedLong(value);
    if (py_int == NULL) return NULL;
    return PyObject_Str(py_int);
}

inline
PyObject* to_str(uint64_t value) {
    auto py_int = PyLong_FromUnsignedLongLong(value);
    if (py_int == NULL) return NULL;
    return PyObject_Str(py_int);
}

inline
PyObject* to_str(double value) {
    auto py_int = PyFloat_FromDouble(value);
    if (py_int == NULL) return NULL;
    return PyObject_Str(py_int);
}

inline
PyObject* to_str(const std::string_view& value) {
    return PyUnicode_FromStringAndSize(value.data(), value.size());
}

inline
PyObject* to_str(const std::wstring_view& value) {
    return PyUnicode_FromWideChar(value.data(), value.size());
}

inline
PyObject* to_str(bool value) {
    return PyUnicode_FromString(value? "true": "false");
}

inline
void raise_error(PyObject* err, const StringView& msg) {
    PyErr_SetString(err, msg.data());
}

inline
void raise_type_error(const Key& key) {
    raise_error(PyExc_TypeError, fmt::format("Invalid nodel::Key type: {}", key.type_name()));
}

inline
void raise_type_error(const Object& obj) {
    raise_error(PyExc_TypeError, fmt::format("Invalid nodel::Object type: {}", obj.type_name()));
}

struct Support
{
    static PyObject* to_str(const Key& key);
    static PyObject* to_str(const Object& obj);
    static PyObject* to_str(const std::pair<Key, Object>& item);

    static PyObject* to_py(const Key& key);

    static Key to_key(PyObject* po);
    static Object to_object(PyObject* po);
};


inline
PyObject* Support::to_str(const Key& key) {
    switch (key.m_repr_ix) {
        case Key::NIL:   return PyUnicode_FromString("nil");
        case Key::BOOL:  return python::to_str(key.m_repr.b);
        case Key::INT:   return python::to_str(key.m_repr.i);
        case Key::UINT:  return python::to_str(key.m_repr.u);
        case Key::FLOAT: return python::to_str(key.m_repr.f);
        case Key::STR: {
            auto& str = key.m_repr.s.data();
            return python::to_str(str);
        }
        default: {
            PyErr_SetString(PyExc_ValueError, "Invalid nodel::Key type");
            return NULL;
        }
    }
}

inline
PyObject* Support::to_str(const Object& obj) {
    switch (obj.m_fields.repr_ix) {
        case Object::NIL:   return PyUnicode_FromString("nil");
        case Object::BOOL:  return python::to_str(obj.m_repr.b);
        case Object::INT:   return python::to_str(obj.m_repr.i);
        case Object::UINT:  return python::to_str(obj.m_repr.u);
        case Object::FLOAT: return python::to_str(obj.m_repr.f);
        case Object::STR: {
            const auto& str = std::get<0>(*obj.m_repr.ps);
            return python::to_str(StringView{str.data(), str.size()});
        }
        case Object::LIST:  [[fallthrough]];
        case Object::MAP:   [[fallthrough]];
        case Object::OMAP:  return python::to_str(obj.to_json());
        case Object::DSRC:  return python::to_str(const_cast<DataSourcePtr>(obj.m_repr.ds)->to_str(obj));
        default: {
            PyErr_SetString(PyExc_ValueError, "Invalid nodel::Object type");
            return NULL;
        }
    }
}

inline
PyObject* Support::to_py(const Key& key) {
    switch (key.m_repr_ix) {
        case Key::NIL:   Py_RETURN_NONE;
        case Key::BOOL:  if (key.m_repr.b) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Key::INT:   return PyLong_FromLongLong(key.m_repr.i);
        case Key::UINT:  return PyLong_FromUnsignedLongLong(key.m_repr.u);
        case Key::FLOAT: return PyFloat_FromDouble(key.m_repr.f);
        case Key::STR: {
            auto& str = key.m_repr.s.data();
            return python::to_str(str);
        }
        default: {
            PyErr_SetString(PyExc_ValueError, "Invalid nodel::Key type");
            return NULL;
        }
    }
}

inline
Key Support::to_key(PyObject* po) {
    if (po == Py_None) {
        return nil;
    } else if (PyUnicode_Check(po)) {
        Py_ssize_t size;
        const char* buf = PyUnicode_AsUTF8AndSize(po, &size);
        return StringView{buf, (StringView::size_type)size};
    } else if (PyLong_Check(po)) {
        int overflow;
        Int v = PyLong_AsLongLongAndOverflow(po, &overflow);
        if (overflow == 0) {
            return v;
        } else if (overflow > 0) {
            PyErr_Clear();
            return PyLong_AsUnsignedLongLong(po);
        } else {
            return nil;
        }
    } else if (PyBool_Check(po)) {
        return po == Py_True;
    } else {
        RefMgr val_type = (PyObject*)PyObject_Type(po);
        RefMgr type_str = PyObject_Str(val_type);
        PyErr_Format(PyExc_TypeError, "%S", (PyObject*)type_str);
        return {};
    }
}

inline
Slice Support::to_slice(PyObject* po) {
    if (PySlice_Check(po)) {
        Py_ssize_t start;
        Py_ssize_t stop;
        Py_ssize_t step;
        int rc = PySlice_Unpack(po, &start, &stop, &step);
    } else {
        return {};
    }
}

inline
Object Support::to_object(PyObject* po) {
    if (po == Py_None) {
        return nil;
    } else if (PyUnicode_Check(po)) {
        Py_ssize_t size;
        const char* buf = PyUnicode_AsUTF8AndSize(po, &size);
        return String{buf, (String::size_type)size};
    } else if (PyLong_Check(po)) {
        int overflow;
        Int v = PyLong_AsLongLongAndOverflow(po, &overflow);
        if (overflow == 0) {
            return v;
        } else if (overflow > 0) {
            PyErr_Clear();
            return PyLong_AsUnsignedLongLong(po);
        } else {
            return Object::INVALID;
        }
    } else if (PyBool_Check(po)) {
        return po == Py_True;
    } else if (PyFloat_Check(po)) {
        return PyFloat_AsDouble(po);
    } else if (PyList_Check(po)) {
        Object list = Object::LIST;
        Py_ssize_t size = PyList_GET_SIZE(po);
        for (Py_ssize_t i=0; i<size; i++) {
            PyObject* py_item = PyList_GET_ITEM(po, i);  // borrowed reference
            Object item = to_object(py_item);
            if (PyErr_Occurred()) return nil;
            list.set(i, item);
        }
        return list;
    } else if (PyDict_Check(po)) {
        Object map = Object::OMAP;
        PyObject* key, *val;
        Py_ssize_t pos = 0;
        while (PyDict_Next(po, &pos, &key, &val)) {  // borrowed references
            auto nd_key = to_key(key);
            if (PyErr_Occurred()) return nil;
            Object nd_val = to_object(val);
            if (PyErr_Occurred()) return nil;
            map.set(nd_key, nd_val);
        }
        return map;
    } else {
        RefMgr val_type = (PyObject*)PyObject_Type(po);
        RefMgr type_str = PyObject_Str(val_type);
        PyErr_Format(PyExc_TypeError, "%S", (PyObject*)type_str);
        return {};
    }
}

} // namespace python
} // namespace nodel

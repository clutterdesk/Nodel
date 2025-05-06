/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <Python.h>
#include <nodel/core/Key.hxx>
#include <nodel/core/Object.hxx>
#include <nodel/support/logging.hxx>

namespace nodel {

namespace python {

class RefMgr
{
  public:
    RefMgr()              : m_ref{nullptr} {}
    RefMgr(PyObject* ref) : m_ref{ref}     {}
    ~RefMgr()                              { Py_XDECREF(m_ref); }

    RefMgr(const RefMgr&) = delete;
    RefMgr(RefMgr&&) = delete;
    RefMgr& operator = (const RefMgr& other) = delete;
    RefMgr& operator = (RefMgr&& other) = delete;

    RefMgr& operator = (PyObject* po) {
        Py_XDECREF(m_ref);
        m_ref = po;
        return *this;
    }

    void clear()            { m_ref = nullptr; }
    PyObject* get() const   { return m_ref; }
    PyObject* get_clear()   { PyObject* ref = get(); clear(); return ref; }
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
    RefMgr py_int = PyLong_FromLong(value);
    if (py_int.get() == NULL) return NULL;
    return PyObject_Str(py_int.get());
}

inline
PyObject* to_str(int64_t value) {
    RefMgr py_int = PyLong_FromLongLong(value);
    if (py_int.get() == NULL) return NULL;
    return PyObject_Str(py_int.get());
}

inline
PyObject* to_str(uint32_t value) {
    RefMgr py_int = PyLong_FromUnsignedLong(value);
    if (py_int.get() == NULL) return NULL;
    return PyObject_Str(py_int.get());
}

inline
PyObject* to_str(uint64_t value) {
    RefMgr py_int = PyLong_FromUnsignedLongLong(value);
    if (py_int.get() == NULL) return NULL;
    return PyObject_Str(py_int.get());
}

inline
PyObject* to_str(double value) {
    RefMgr py_int = PyFloat_FromDouble(value);
    if (py_int.get() == NULL) return NULL;
    return PyObject_Str(py_int.get());
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

class PyOpaque : public nodel::Opaque
{
  public:
    PyOpaque(PyObject* po) : m_po(po) { Py_XINCREF(po); }
    ~PyOpaque() override { Py_XDECREF(m_po); m_po = nullptr; }

    Opaque* clone() override { return new PyOpaque{m_po}; }

    nodel::String str() override {
        RefMgr r_str{PyObject_Str(m_po)};
        Py_ssize_t c_str_len;
        auto c_str = PyUnicode_AsUTF8AndSize(r_str.get(), &c_str_len);
        if (c_str_len == -1) throw std::runtime_error{"PyUnicode_AsUTF8AndSize failed unexpectedly"};
        return {c_str, (std::string::size_type)c_str_len};
    }

    nodel::String repr() override {
        RefMgr r_str{PyObject_Repr(m_po)};
        Py_ssize_t c_str_len;
        auto c_str = PyUnicode_AsUTF8AndSize(r_str.get(), &c_str_len);
        if (c_str_len == -1) throw std::runtime_error{"PyUnicode_AsUTF8AndSize failed unexpectedly"};
        return {c_str, (std::string::size_type)c_str_len};
    }

    PyObject* m_po;
};

struct Support
{
    static PyObject* to_str(const Key& key);
    static PyObject* to_str_repr(const Object& obj, bool repr);
    static PyObject* to_str(const std::pair<Key, Object>& item);

    static PyObject* to_num(const Object& obj);
    static PyObject* to_py(const Key& key);
    static PyObject* to_py(const Object& obj);

    static double to_double(PyObject* po);

    static Key to_key(PyObject* po);
    static Slice to_slice(PyObject* po);
    static Object to_object(PyObject* po);

    static void clear_parent(const Object&);
};


inline
PyObject* Support::to_str(const Key& key) {
    switch (key.m_repr_ix) {
        case Key::NIL:   return PyUnicode_FromString("None");
        case Key::BOOL:  return PyUnicode_FromString((key.m_repr.b)? "True": "False");
        case Key::INT:   return python::to_str(key.m_repr.i);
        case Key::UINT:  return python::to_str(key.m_repr.u);
        case Key::FLOAT: return python::to_str(key.m_repr.f);
        case Key::STR: {
            auto& str = key.m_repr.s.data();
            return python::to_str(str);
        }
        default: {
            raise_type_error(key);
            return NULL;
        }
    }
}

inline
PyObject* Support::to_str_repr(const Object& obj, bool repr) {
    switch (obj.m_fields.repr_ix) {
        case Object::NIL:   return PyUnicode_FromString("None");
        case Object::BOOL:  return PyUnicode_FromString((obj.m_repr.b)? "True": "False");
        case Object::INT:   return python::to_str(obj.m_repr.i);
        case Object::UINT:  return python::to_str(obj.m_repr.u);
        case Object::FLOAT: return python::to_str(obj.m_repr.f);
        case Object::STR: {
            const auto& str = std::get<0>(*obj.m_repr.ps);
            if (repr) {
                std::stringstream ss;
                ss << std::quoted(str);
                auto str = ss.str();  // TODO: use ss.view() when available
                return python::to_str(StringView{str.data(), str.size()});
            } else {
                return python::to_str(StringView{str.data(), str.size()});
            }
        }
        case Object::LIST:  [[fallthrough]];
        case Object::SMAP:  [[fallthrough]];
        case Object::OMAP: {
            auto str = obj.to_json();
            return python::to_str(StringView{str.data(), str.size()});
        }
        case Object::ANY:   return repr? PyObject_Repr(obj.as<PyOpaque>().m_po): PyObject_Str(obj.as<PyOpaque>().m_po);
        case Object::DSRC:  {
            if (repr) return python::to_str(const_cast<DataSourcePtr>(obj.m_repr.ds)->to_json(obj));
            else return python::to_str(const_cast<DataSourcePtr>(obj.m_repr.ds)->to_str(obj));
        }
        default: {
            raise_type_error(obj);
            return NULL;
        }
    }
}

inline
PyObject* Support::to_num(const Object& obj) {
    switch (obj.m_fields.repr_ix) {
        //case Object::BOOL:  if (obj.m_repr.b) Py_RETURN_ONE; else Py_RETURN_ZERO;
        case Object::INT:   return PyLong_FromLongLong(obj.m_repr.i);
        case Object::UINT:  return PyLong_FromUnsignedLongLong(obj.m_repr.u);
        case Object::FLOAT: return PyFloat_FromDouble(obj.m_repr.f);
        default: {
            PyErr_SetString(PyExc_ValueError, "nodel::Object is not a number");
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
            raise_type_error(key);
            return NULL;
        }
    }
}

inline
PyObject* Support::to_py(const Object& obj) {
    switch (obj.m_fields.repr_ix) {
        case Object::NIL:   Py_RETURN_NONE;
        case Object::BOOL:  if (obj.m_repr.b) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Object::INT:   return PyLong_FromLongLong(obj.m_repr.i);
        case Object::UINT:  return PyLong_FromUnsignedLongLong(obj.m_repr.u);
        case Object::FLOAT: return PyFloat_FromDouble(obj.m_repr.f);
        case Object::STR: {
            auto& str = std::get<0>(*obj.m_repr.ps);
            return python::to_str(str);
        }
        case Object::LIST: {
            Py_ssize_t size = obj.size();
            PyObject* py_list = PyList_New(size);
            if (py_list == NULL) return NULL;
            for (Py_ssize_t i = 0; i < size; ++i) {
                PyObject* item = to_py(obj.get(i));
                if (item == NULL) {
                    Py_DECREF(py_list);
                    return NULL;
                }
                PyList_SET_ITEM(py_list, i, item);  // steals reference
            }
            return py_list;
        }
        case Object::ANY: {
            PyObject* po = obj.as<PyOpaque>().m_po;
            Py_INCREF(po);
            return po;
        }
        case Object::OMAP:
        case Object::SMAP:
        default: {
            raise_type_error(obj);
            return NULL;
        }
    }
}

inline
double Support::to_double(PyObject* po) {
    if (PyFloat_Check(po)) {
        return PyFloat_AsDouble(po);
    } else if (PyLong_Check(po)) {
        RefMgr float_ref = PyNumber_Float(po);
        return PyFloat_AsDouble(float_ref.get());
    } else if (NodelObject_CheckExact(po)) {
        NodelObject* nd_obj = (NodelObject*)po;
        return nd_obj->obj.as<double>();
    } else {
        return 0;
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
        if (rc == -1) return {};
        if (step > 0) {
            auto ep_start = (start == std::numeric_limits<Py_ssize_t>::min())?
                           Endpoint{nil, Endpoint::Kind::CLOSED}:
                           Endpoint{start, Endpoint::Kind::CLOSED};
            auto ep_stop = (stop == std::numeric_limits<Py_ssize_t>::max())?
                          Endpoint{nil, Endpoint::Kind::OPEN}:
                          Endpoint{stop, Endpoint::Kind::OPEN};
            return {ep_start, ep_stop, step};
        } else {
            auto ep_start = (start == std::numeric_limits<Py_ssize_t>::max())?
                             Endpoint{nil, Endpoint::Kind::CLOSED}:
                             Endpoint{start, Endpoint::Kind::CLOSED};
            auto ep_stop = (stop == std::numeric_limits<Py_ssize_t>::min())?
                           Endpoint{nil, Endpoint::Kind::OPEN}:
                           Endpoint{stop, Endpoint::Kind::OPEN};
            return {ep_start, ep_stop, step};
        }
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
    } else if (PyLong_CheckExact(po)) {
        int overflow;
        Int v = PyLong_AsLongLongAndOverflow(po, &overflow);
        if (overflow == 0) {
            return v;
        } else if (overflow > 0) {
            PyErr_Clear();
            return PyLong_AsUnsignedLongLong(po);
        } else {
            return make_error("Integer conversion error");
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
    } else if (NodelObject_CheckExact(po)) {
        return ((NodelObject*)po)->obj;
    } else {
        return std::unique_ptr<nodel::Opaque>{new PyOpaque{po}};
    }
}

inline
void Support::clear_parent(const Object& object) {
    object.clear_parent();
}

} // namespace python
} // namespace nodel

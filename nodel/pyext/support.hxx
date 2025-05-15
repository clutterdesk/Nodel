/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <Python.h>
#include <string>
#include <stdexcept>
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
PyObject* unicode_slice(PyObject *str, PyObject *slice)
{
    if (!PyUnicode_Check(str)) {
        PyErr_SetString(PyExc_TypeError, "Expected a Unicode string");
        return NULL;
    }

    if (!PySlice_Check(slice)) {
        PyErr_SetString(PyExc_TypeError, "Expected a slice object");
        return NULL;
    }

    Py_ssize_t start, stop, step, slicelength;
    Py_ssize_t length = PyUnicode_GET_LENGTH(str);

    if (PySlice_GetIndicesEx(slice, length, &start, &stop, &step, &slicelength) < 0)
        return NULL;

    if (slicelength <= 0)
        return PyUnicode_New(0, 0);

    if (PyUnicode_READY(str) < 0)
        return NULL;

    // Get kind and data pointers
    int kind = PyUnicode_KIND(str);
    void *data = PyUnicode_DATA(str);

    // Get maximum character (used to determine internal encoding)
    Py_UCS4 maxchar = PyUnicode_MAX_CHAR_VALUE(str);

    // Create a new Unicode object for the result
    PyObject *result = PyUnicode_New(slicelength, maxchar);
    if (result == NULL) return NULL;

    void *res_data = PyUnicode_DATA(result);

    // Copy characters with the specified step
    for (Py_ssize_t i = 0, idx = start; i < slicelength; i++, idx += step) {
        PyUnicode_WRITE(kind, res_data, i, PyUnicode_READ(kind, data, idx));
    }

    return result;
}

inline
String assign_slice(const String& input, PyObject* slice, PyObject* replace) {
    if (!PySlice_Check(slice)) {
        PyErr_SetString(PyExc_TypeError, "Expected a slice object");
        return NULL;
    }

    if (!PyUnicode_Check(replace)) {
        PyErr_SetString(PyExc_TypeError, "Expected a unicode object as the replacement");
        return NULL;
    }

    // Convert PyUnicode to UTF-8 std::string
    Py_ssize_t repl_size;
    auto repl_bytes = PyUnicode_AsUTF8AndSize(replace, &repl_size);
    if (!repl_bytes) {
        PyErr_SetString(PyExc_TypeError, "Failed to extract UTF-8 from replacement string");
        return NULL;
    }

    StringView repl_view{repl_bytes, static_cast<StringView::size_type>(repl_size)};

    Py_ssize_t start, stop, step;
    if (PySlice_Unpack(slice, &start, &stop, &step) < 0)
        return NULL;

    Py_ssize_t length = static_cast<Py_ssize_t>(input.size());
    auto slice_len = PySlice_AdjustIndices(length, &start, &stop, step);

    if (step == 1) {
        String result;
        result.reserve(input.size() - slice_len + result.size());
        result.append(input.begin(), input.begin() + start);
        result.append(repl_view);
        result.append(input.begin() + stop, input.end());
        return result;
    } else {
        auto min_len = std::min(slice_len, repl_size);
        String result;
        Py_ssize_t repl_pos = 0;
        Py_ssize_t pos = start;
        for (; repl_pos < min_len; ++repl_pos, pos += step) {
            if (pos >= result.size())
                result.append(input.begin() + result.size(), input.begin() + pos + 1);
            result[pos] = repl_bytes[repl_pos];
        }
        auto in_pos = result.size();
        for (; repl_pos < slice_len; ++repl_pos, pos += step) {
            result.append(input.begin() + in_pos, input.begin() + pos);
            in_pos = pos + 1;
        }
        if (in_pos < input.size())
            result.append(input.begin() + in_pos, input.end());
        return result;
    }
}

inline
std::string_view to_string_view(PyObject* po) {
    if (PyUnicode_Check(po)) {
        // Lifetime of utf8 encoded string is managed by Python, so the stringview that
        // is returned should not be held beyond the lifetime of the argument.
        Py_ssize_t size;
        const char* str = PyUnicode_AsUTF8AndSize(po, &size);
        if (str != nullptr) {
            return {str, (std::string_view::size_type)size};
        }
    }
    return "";
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


class PyAlien : public nodel::Alien
{
  public:
    PyAlien(PyObject* po) : m_po(po) { Py_XINCREF(po); }
    ~PyAlien() override { Py_XDECREF(m_po); m_po = nullptr; }

    std::unique_ptr<Alien> clone() override { return std::make_unique<PyAlien>(m_po); }

    nodel::String to_str() const override {
        RefMgr r_str{PyObject_Str(m_po)};
        Py_ssize_t c_str_len;
        auto c_str = PyUnicode_AsUTF8AndSize(r_str.get(), &c_str_len);
        if (c_str_len == -1) throw std::runtime_error{"PyUnicode_AsUTF8AndSize failed unexpectedly"};
        return {c_str, (std::string::size_type)c_str_len};
    }

    nodel::String to_json() const override {
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
    static PyObject* prepare_return_value(const Object& obj);

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
        case Object::ANY:   return repr? PyObject_Repr(obj.as<PyAlien>().m_po): PyObject_Str(obj.as<PyAlien>().m_po);
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
    switch (obj.type()) {
        case Object::NIL:   Py_RETURN_NONE;
        case Object::BOOL:  if (obj.m_repr.b) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Object::INT:   return PyLong_FromLongLong(obj.as<Int>());
        case Object::UINT:  return PyLong_FromUnsignedLongLong(obj.as<UInt>());
        case Object::FLOAT: return PyFloat_FromDouble(obj.as<Float>());
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
            PyObject* po = obj.as<PyAlien>().m_po;
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
PyObject* Support::prepare_return_value(const Object& obj) {
    switch (obj.type()) {
        case Object::NIL:   Py_RETURN_NONE;
        case Object::BOOL:  if (obj.as<bool>()) Py_RETURN_TRUE; else Py_RETURN_FALSE;
        case Object::INT:   return PyLong_FromLongLong(obj.as<Int>());
        case Object::UINT:  return PyLong_FromUnsignedLongLong(obj.as<UInt>());
        case Object::FLOAT: return PyFloat_FromDouble(obj.as<Float>());
        case Object::STR:   [[fallthrough]];
        case Object::LIST:  [[fallthrough]];
        case Object::OMAP:  [[fallthrough]];
        case Object::SMAP:  return (PyObject*)NodelObject_wrap(obj);
        case Object::ANY: {
            PyObject* po = obj.as<PyAlien>().m_po;
            Py_INCREF(po);
            return po;
        }
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
        return std::unique_ptr<nodel::Alien>{new PyAlien{po}};
    }
}

inline
void Support::clear_parent(const Object& object) {
    object.clear_parent();
}

} // namespace python
} // namespace nodel

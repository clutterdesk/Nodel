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

#include <nodel/parser/json.hxx>
#include <nodel/support/logging.hxx>

#include <cmath>

extern "C" {

using namespace nodel;
using RefMgr = python::RefMgr;

static python::Support support;


//-----------------------------------------------------------------------------
// Miscellaneous Functions
//-----------------------------------------------------------------------------

static bool require_any_int(const Object& obj) {
    if (!nodel::is_integer(obj)) {
        python::raise_type_error(obj);
        return false;
    }
    return true;
}

static bool require_number(const Object& obj) {
    if (!nodel::is_number(obj)) {
        python::raise_type_error(obj);
        return false;
    }
    return true;
}

static bool require_container(const Object& obj) {
    if (!nodel::is_container(obj)) {
        python::raise_type_error(obj);
        return false;
    }
    return true;
}

static bool require_sequence(const Object& obj) {
    if (obj.type() != Object::LIST && !obj.is_type<nodel::String>()) {
        python::raise_type_error(obj);
        return false;
    }
    return true;
}

static bool require_subscript(const Object& obj, const Key& key) {
    if (obj.is_type<ObjectList>() || obj.is_type<String>()) {
        if (!nodel::is_integer(key)) {
            python::raise_type_error(key);
            return false;
        }
    } else if (!nodel::is_map(obj)) {
        python::raise_type_error(obj);
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
// NodelObject Number Protocol
//
// In the future, a better implementation would be to precompile the python code and
// convert these operations by first casting the NodelObject to a number.
//-----------------------------------------------------------------------------

static PyObject* num_to_py(NodelObject* nd_obj) {
    auto& obj = nd_obj->obj;
    switch (obj.type()) {
        case Object::INT:   return PyLong_FromLongLong(obj.as<Int>());
        case Object::UINT:  return PyLong_FromUnsignedLongLong(obj.as<UInt>());
        case Object::FLOAT: return PyFloat_FromDouble(obj.as<Float>());
        default: {
            python::raise_type_error(obj);
            return NULL;
        }
    }
}

static PyObject* add(NodelObject* nd_obj, PyObject* po, bool commute) {
    auto type = nd_obj->obj.type();
    if (type == Object::STR) {
        RefMgr lhs = support.to_py(nd_obj->obj);
        return commute? PyUnicode_Concat(po, lhs): PyUnicode_Concat(lhs, po);
    } else if (type == Object::LIST) {
        Object rhs;
        if (PyList_Check(po)) {
            rhs = support.to_object(po);
        } else if (NodelObject_CheckExact(po)) {
            rhs = ((NodelObject*)po)->obj;
        } else {
            PyErr_SetString(PyExc_TypeError, "Expected list object");
            return NULL;
        }

        Object new_list{Object::LIST};
        if (commute) {
            for (const auto& val : rhs.iter_values())
                new_list.append(val);
            for (const auto& val : nd_obj->obj.iter_values())
                new_list.append(val);
        } else {
            for (const auto& val : nd_obj->obj.iter_values())
                new_list.append(val);
            for (const auto& val : rhs.iter_values())
                new_list.append(val);
        }
        return (PyObject*)NodelObject_wrap(new_list);
    } else {
        RefMgr val{num_to_py(nd_obj)};
        if (val.get() == NULL) return NULL;
        return PyNumber_Add(val, po);
    }
}

static PyObject* NodelObject_add(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1))
        return add((NodelObject*)arg1, arg2, false);

    if (NodelObject_CheckExact(arg2))
        return add((NodelObject*)arg2, arg1, true);

    PyErr_SetString(PyExc_TypeError, "Invalid type");
    return NULL;
}

static PyObject* NodelObject_subtract(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Subtract(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Subtract(arg1, val);
    }
}

static PyObject* NodelObject_sq_repeat(PyObject* self, Py_ssize_t count);

static PyObject* NodelObject_multiply(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        if (nodel::is_number(nd_obj)) {
            PyObject* val = num_to_py(nd_obj);
            if (val == NULL) return NULL;
            return PyNumber_Multiply(val, arg2);
        } else if (nd_obj->obj.type() == Object::LIST) {
            Py_ssize_t repeats = PyLong_AsUnsignedLongLong(arg2);
            return NodelObject_sq_repeat(arg1, repeats);
        } else {
            python::raise_type_error(nd_obj->obj);
            return NULL;
        }
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Multiply(arg1, val);
    }
}

static PyObject* NodelObject_remainder(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Remainder(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Remainder(arg1, val);
    }
}

static PyObject* NodelObject_divmod(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Divmod(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Divmod(arg1, val);
    }
}

static PyObject* NodelObject_power(PyObject* arg1, PyObject* arg2, PyObject* arg3) {
    if (NodelObject_CheckExact(arg3)) {
        RefMgr ref = num_to_py((NodelObject*)arg3);
        return PyNumber_Power(arg1, arg2, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Power(val, arg2, arg3);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Power(arg1, val, arg3);
    }
}

static PyObject* NodelObject_negative(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;

    switch (self_obj.type()) {
        case Object::INT: return PyLong_FromLongLong(-self_obj.as<Int>());
        case Object::UINT: return PyLong_FromLongLong(-self_obj.as<UInt>());
        case Object::FLOAT: return PyFloat_FromDouble(-self_obj.as<Float>());
        default:
            python::raise_error(PyExc_ValueError, "Invalid operation");
            return NULL;
    }
}

static PyObject* NodelObject_positive(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;

    switch (self_obj.type()) {
        case Object::INT: return PyLong_FromLongLong(self_obj.as<Int>());
        case Object::UINT: return PyLong_FromUnsignedLongLong(self_obj.as<UInt>());
        case Object::FLOAT: return PyFloat_FromDouble(self_obj.as<Float>());
        default:
            python::raise_error(PyExc_ValueError, "Invalid operation");
            return NULL;
    }
}

static PyObject* NodelObject_absolute(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;

    switch (self_obj.type()) {
        case Object::INT: return PyLong_FromLongLong(std::abs(self_obj.as<Int>()));
        case Object::UINT: return PyLong_FromUnsignedLongLong(self_obj.as<UInt>());
        case Object::FLOAT: return PyFloat_FromDouble(std::abs(self_obj.as<Float>()));
        default:
            python::raise_error(PyExc_ValueError, "Invalid operation");
            return NULL;
    }
}

static int NodelObject_bool(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    return self_obj.cast<bool>()? 1: 0;
}

static PyObject* NodelObject_invert(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;

    switch (self_obj.type()) {
        case Object::INT: return PyLong_FromLongLong(~self_obj.as<Int>());
        case Object::UINT: return PyLong_FromUnsignedLongLong(~self_obj.as<UInt>());
        default:
            python::raise_error(PyExc_ValueError, "Invalid operation");
            return NULL;
    }
}

static PyObject* NodelObject_lshift(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Lshift(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Lshift(arg1, val);
    }
}

static PyObject* NodelObject_rshift(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Rshift(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Rshift(arg1, val);
    }
}

static PyObject* NodelObject_and(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_And(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_And(arg1, val);
    }
}

static PyObject* NodelObject_xor(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Xor(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Xor(arg1, val);
    }
}

static PyObject* NodelObject_or(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Or(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_Or(arg1, val);
    }
}

static PyObject* NodelObject_int(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (self_obj.is_type<UInt>()) {
        return PyLong_FromUnsignedLongLong(self_obj.as<UInt>());
    } else {
        try {
            return PyLong_FromLongLong(self_obj.cast<Int>());
        } catch (const WrongType& exc) {
            python::raise_type_error(self_obj);
        }
    }
    return NULL;
}

static PyObject* NodelObject_float(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    try {
        return PyFloat_FromDouble(self_obj.cast<Float>());
    } catch (const WrongType& exc) {
        python::raise_type_error(self_obj);
    }
    return NULL;
}

static PyObject* inplace_add_string(NodelObject* arg1, PyObject* arg2) {
    RefMgr ref;
    if (!PyUnicode_Check(arg2)) {
        ref = PyObject_Str(arg2);
        arg2 = ref.get();
    }
    arg1->obj.as<String>().append(python::to_string_view(arg2));
    Py_INCREF(arg1);
    return (PyObject*)arg1;
}

static PyObject* inplace_add_list(NodelObject* arg1, PyObject* arg2) {
    auto& obj = arg1->obj;

    Object rhs;
    if (PyList_Check(arg2)) {
        rhs = support.to_object(arg2);
    } else if (NodelObject_CheckExact(arg2)) {
        rhs = ((NodelObject*)arg2)->obj;
    } else {
        PyErr_SetString(PyExc_TypeError, "Expected list object");
        return NULL;
    }

    for (const auto& val : rhs.iter_values())
        obj.append(val);
    Py_INCREF(arg1);
    return (PyObject*)arg1;
}

static PyObject* NodelObject_inplace_add(PyObject* arg1, PyObject* arg2) {
    ASSERT(NodelObject_CheckExact(arg1));
    auto nd_obj = (NodelObject*)arg1;
    auto& obj = nd_obj->obj;
    auto type = obj.type();
    if (type == Object::STR) {
        return inplace_add_string(nd_obj, arg2);
    } else if (type == Object::LIST) {
        return inplace_add_list(nd_obj, arg2);
    } else if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_add(arg1, ref.get());
    } else {
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() += PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<Int>() + PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() += PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<UInt>() + PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::FLOAT: {
                obj.as<Float>() += support.to_double(arg2);
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    }
}

static PyObject* NodelObject_inplace_subtract(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_subtract(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() -= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<Int>() - PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() -= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<UInt>() - PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::FLOAT: {
                obj.as<Float>() -= support.to_double(arg2);
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceSubtract(arg1, ref.get());
    }
}

static PyObject* NodelObject_inplace_multiply(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_multiply(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() *= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<Int>() * PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() *= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<UInt>() * PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::FLOAT: {
                obj.as<Float>() *= support.to_double(arg2);
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceMultiply(arg1, ref.get());
    }
}

static PyObject* NodelObject_inplace_remainder(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_remainder(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() %= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    auto y = PyFloat_AsDouble(arg2);
                    auto rem = std::remainder(obj.as<Int>(), y);
                    obj = (rem < 0)? rem + y: rem;
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() %= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    auto y = PyFloat_AsDouble(arg2);
                    auto rem = std::remainder(obj.as<UInt>(), y);
                    obj = (rem < 0)? rem + y: rem;
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::FLOAT: {
                if (PyLong_Check(arg2)) {
                    auto y = PyLong_AsLongLong(arg2);
                    auto rem = std::remainder(obj.as<Float>(), y);
                    obj = (rem < 0)? rem + y: rem;
                    break;
                } else if (PyFloat_Check(arg2)) {
                    auto y = PyFloat_AsDouble(arg2);
                    auto rem = std::remainder(obj.as<UInt>(), y);
                    obj = (rem < 0)? rem + y: rem;
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceRemainder(arg1, ref.get());
    }
}

static PyObject* NodelObject_inplace_power(PyObject* arg1, PyObject* arg2, PyObject* arg3) {
    if (NodelObject_CheckExact(arg1)) {
        PyObject* result = NodelObject_power(arg1, arg2, arg3);
        if (result == NULL) return NULL;
        auto nd_obj = (NodelObject*)arg1;
        nd_obj->obj = support.to_object(result);
        Py_INCREF(arg1);
        return arg1;
    } else {
        return NodelObject_power(arg1, arg2, arg3);
    }
}

static PyObject* NodelObject_inplace_lshift(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_lshift(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() <<= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() <<= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceLshift(arg1, ref.get());
    }
}

static PyObject* NodelObject_inplace_rshift(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
            RefMgr ref = num_to_py((NodelObject*)arg2);
            return NodelObject_inplace_rshift(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() >>= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() >>= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceRshift(arg1, ref.get());
    }
}

static PyObject* NodelObject_inplace_and(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_and(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() &= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() &= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceAnd(arg1, ref.get());
    }
}

static PyObject* NodelObject_inplace_xor(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_xor(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() ^= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() ^= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceXor(arg1, ref.get());
    }
}

static PyObject* NodelObject_inplace_or(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_or(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() |= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() |= PyLong_AsLongLong(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceOr(arg1, ref.get());
    }
}

static PyObject* NodelObject_floor_divide(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_FloorDivide(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_FloorDivide(arg1, val);
    }
}

static PyObject* NodelObject_true_divide(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg1)) {
        NodelObject* nd_obj = (NodelObject*)arg1;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_TrueDivide(val, arg2);
    } else {
        NodelObject* nd_obj = (NodelObject*)arg2;
        PyObject* val = num_to_py(nd_obj);
        if (val == NULL) return NULL;
        return PyNumber_TrueDivide(arg1, val);
    }
}

static PyObject* NodelObject_inplace_floor_divide(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_floor_divide(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Int>() /= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = std::floor(obj.as<Int>() / PyFloat_AsDouble(arg2));
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() /= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = std::floor(obj.as<UInt>() / PyFloat_AsDouble(arg2));
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::FLOAT: {
                if (PyLong_Check(arg2)) {
                    obj = std::floor(obj.as<Float>() / PyLong_AsLongLong(arg2));
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = std::floor(obj.as<Float>() / PyFloat_AsDouble(arg2));
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceFloorDivide(arg1, ref.get());
    }
}

static PyObject* NodelObject_inplace_true_divide(PyObject* arg1, PyObject* arg2) {
    if (NodelObject_CheckExact(arg2)) {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return NodelObject_inplace_true_divide(arg1, ref.get());
    } else if (NodelObject_CheckExact(arg1)) {
        auto nd_obj = (NodelObject*)arg1;
        auto& obj = nd_obj->obj;
        switch (obj.type()) {
            case Object::INT: {
                if (PyLong_Check(arg2)) {
                    obj = obj.as<Int>() / (double)PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<Int>() / PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::UINT: {
                if (PyLong_Check(arg2)) {
                    obj.as<UInt>() /= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<UInt>() / PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            case Object::FLOAT: {
                if (PyLong_Check(arg2)) {
                    obj.as<Float>() /= PyLong_AsLongLong(arg2);
                    break;
                } else if (PyFloat_Check(arg2)) {
                    obj = obj.as<Float>() / PyFloat_AsDouble(arg2);
                    break;
                } else {
                    python::raise_error(PyExc_ValueError, "Invalid argument type");
                    return NULL;
                }
                break;
            }
            default: {
                python::raise_error(PyExc_ValueError, "Invalid operation");
                return NULL;
            }
        }
        Py_INCREF(arg1);
        return arg1;
    } else {
        RefMgr ref = num_to_py((NodelObject*)arg2);
        return PyNumber_InPlaceTrueDivide(arg1, ref.get());
    }
}

static PyObject* NodelObject_index(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_any_int(self_obj)) return NULL;
    if (self_obj.is_type<UInt>()) {
        return PyLong_FromUnsignedLongLong(self_obj.as<UInt>());
    } else {
        try {
            return PyLong_FromLongLong(self_obj.cast<Int>());
        } catch (const WrongType& exc) {
            python::raise_type_error(self_obj);
        }
    }
    return NULL;
}

static PyObject* NodelObject_matrix_multiply(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}

static PyObject* NodelObject_inplace_matrix_multiply(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_number(self_obj)) return NULL;
    python::raise_error(PyExc_RuntimeError, "Not implemented, yet");
    return NULL;
}


static PyNumberMethods NodelObject_as_number = {
    .nb_add = NodelObject_add,
    .nb_subtract = NodelObject_subtract,
    .nb_multiply = NodelObject_multiply,
    .nb_remainder = NodelObject_remainder,
    .nb_divmod = NodelObject_divmod,
    .nb_power = NodelObject_power,
    .nb_negative = NodelObject_negative,
    .nb_positive = NodelObject_positive,
    .nb_absolute = NodelObject_absolute,
    .nb_bool = NodelObject_bool,
    .nb_invert = NodelObject_invert,
    .nb_lshift = NodelObject_lshift,
    .nb_rshift = NodelObject_rshift,
    .nb_and = NodelObject_and,
    .nb_xor = NodelObject_xor,
    .nb_or = NodelObject_or,
    .nb_int = NodelObject_int,
    .nb_float = NodelObject_float,
    .nb_inplace_add = NodelObject_inplace_add,
    .nb_inplace_subtract = NodelObject_inplace_subtract,
    .nb_inplace_multiply = NodelObject_inplace_multiply,
    .nb_inplace_remainder = NodelObject_inplace_remainder,
    .nb_inplace_power = NodelObject_inplace_power,
    .nb_inplace_lshift = NodelObject_inplace_lshift,
    .nb_inplace_rshift = NodelObject_inplace_rshift,
    .nb_inplace_and = NodelObject_inplace_and,
    .nb_inplace_xor = NodelObject_inplace_xor,
    .nb_inplace_or = NodelObject_inplace_or,
    .nb_floor_divide = NodelObject_floor_divide,
    .nb_true_divide = NodelObject_true_divide,
    .nb_inplace_floor_divide = NodelObject_inplace_floor_divide,
    .nb_inplace_true_divide = NodelObject_inplace_true_divide,
    .nb_index = NodelObject_index,
    .nb_matrix_multiply = NodelObject_matrix_multiply,
    .nb_inplace_matrix_multiply = NodelObject_inplace_matrix_multiply,
};


//-----------------------------------------------------------------------------
// NodelObject Mapping Protocol
//-----------------------------------------------------------------------------

static Py_ssize_t NodelObject_mp_length(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    try {
        if (!nodel::is_container(self_obj) && self_obj.type() != Object::STR) {
            python::raise_type_error(self_obj);
            return -1;
        }
        return (Py_ssize_t)self_obj.size();
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return -1;
    }
}

static PyObject* NodelObject_mp_subscript(PyObject* self, PyObject* key) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;

    if (!nodel::is_container(self_obj) && self_obj.type() != Object::STR) {
        python::raise_type_error(self_obj);
        return NULL;
    }

    try {
        if (PySlice_Check(key)) {
            if (self_obj.type() == Object::STR) {
                auto po = support.to_py(self_obj);
                return python::unicode_slice(po, key);
            } else {
                auto slice = support.to_slice(key);
                return (PyObject*)NodelObject_wrap(self_obj.get(slice));
            }
        } else {
            auto nd_key = support.to_key(key);
            if (!require_subscript(self_obj, nd_key)) return NULL;
            return support.prepare_return_value(self_obj.get(nd_key));
        }
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}

static int NodelObject_mp_ass_sub(PyObject* self, PyObject* key, PyObject* value) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;

    if (!nodel::is_container(self_obj) && self_obj.type() != Object::STR) {
        python::raise_type_error(self_obj);
        return -1;
    }

    try
    {
        if (PySlice_Check(key)) {
            if (value == NULL) {
                if (self_obj.type() == Object::STR) {
                    RefMgr empty_str = PyUnicode_FromString("");
                    self_obj.as<String>() = python::assign_slice(self_obj.as<String>(), key, empty_str);
                } else {
                    auto slice = support.to_slice(key);
                    self_obj.del(slice);
                }
            } else {
                if (self_obj.type() == Object::STR) {
                    self_obj.as<String>() = python::assign_slice(self_obj.as<String>(), key, value);
                } else {
                    auto slice = support.to_slice(key);
                    auto nd_vals = support.to_object(value);
                    for (auto& nd_val : nd_vals.iter_values())
                        support.clear_parent(nd_val);  // prevent copying
                    self_obj.set(slice, nd_vals);
                }
            }
        } else {
            auto nd_key = support.to_key(key);
            if (!require_subscript(self_obj, nd_key)) return -1;
            if (value == NULL) {
                self_obj.del(nd_key);
            } else {
                Object nd_val;
                if (Py_IS_TYPE(value, &NodelObjectType)) {
                    nd_val = ((NodelObject*)value)->obj;
                } else {
                    nd_val = support.to_object(value);
                    if (PyErr_Occurred()) return -1;
                }
                self_obj.set(nd_key, nd_val);
            }
        }
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return -1;
    }
    return 0;
}

static PyMappingMethods NodelObject_as_mapping = {
    .mp_length = (lenfunc)NodelObject_mp_length,
    .mp_subscript = (binaryfunc)NodelObject_mp_subscript,
    .mp_ass_subscript = (objobjargproc)NodelObject_mp_ass_sub
};


//-----------------------------------------------------------------------------
// NodelObject Sequence Protocol
//-----------------------------------------------------------------------------

static Py_ssize_t NodelObject_sq_length(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (!require_sequence(self_obj)) return -1;
    return (Py_ssize_t)self_obj.size();
}

static PyObject* NodelObject_sq_concat(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    RefMgr lhs = support.to_py(nd_self->obj);
    if (!lhs.get()) return NULL;

    if (NodelObject_CheckExact(arg)) {
        NodelObject* nd_arg = (NodelObject*)arg;
        RefMgr rhs = support.to_py(nd_arg->obj);
        if (!rhs.get()) return NULL;
        return PySequence_Concat(lhs.get(), rhs.get());
    } else {
        return PySequence_Concat(lhs.get(), arg);
    }
}

static PyObject* NodelObject_sq_repeat(PyObject* self, Py_ssize_t count) {
    NodelObject* nd_self = (NodelObject*)self;
    RefMgr py_obj = support.to_py(nd_self->obj);
    if (py_obj.get() == nullptr) return NULL;
    return PySequence_Repeat(py_obj.get(), count);
}

static PyObject* NodelObject_sq_item(PyObject* self, Py_ssize_t index) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    if (index == std::numeric_limits<Py_ssize_t>::max() || (size_t)index >= self_obj.size())
        return NULL;
    return (PyObject*)NodelObject_wrap(self_obj.get(index));
}

static int NodelObject_sq_ass_item(PyObject* self, Py_ssize_t index, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    Object value = support.to_object(arg);
    if (value.is_empty()) return -1;
    nd_self->obj.set(index, value);
    return 0;
}

static int NodelObject_sq_contains(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;
    auto obj = support.to_object(arg);
    return self_obj.contains(obj);
}

// TODO: AI needs checking
static PyObject* NodelObject_sq_inplace_concat(PyObject* self, PyObject* arg) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& obj = nd_self->obj;
    auto type = obj.type();

    if (type == Object::STR) {
        // In-place string concatenation
        RefMgr ref;
        if (PyUnicode_Check(arg)) {
            obj.as<String>().append(python::to_string_view(arg));
            Py_INCREF(self);
            return self;
        } else {
            PyErr_SetString(PyExc_TypeError, "Invalid type for string concatenation");
            return NULL;
        }
    } else if (type == Object::LIST) {
        // In-place list concatenation
        Object rhs;
        if (PyList_Check(arg)) {
            rhs = support.to_object(arg);
        } else if (NodelObject_CheckExact(arg)) {
            rhs = ((NodelObject*)arg)->obj;
        } else {
            PyErr_SetString(PyExc_TypeError, "Expected list object");
            return NULL;
        }
        for (const auto& val : rhs.iter_values())
            obj.append(val);
        Py_INCREF(self);
        return self;
    } else {
        PyErr_SetString(PyExc_TypeError, "In-place concat only supported for list or string");
        return NULL;
    }
}

static PyObject* NodelObject_sq_inplace_repeat(PyObject* self, Py_ssize_t count) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& obj = nd_self->obj;
    auto type = obj.type();

    if (type == Object::STR) {
        auto& str = obj.as<String>();
        String result;
        for (Py_ssize_t i = 0; i < count; ++i)
            result.append(str);
        str = std::move(result);
    } else if (type == Object::LIST) {
        ObjectList& list = obj.as<ObjectList>();
        ObjectList original = list;
        for (Py_ssize_t i = 1; i < count; ++i)
            for (const auto& item : original)
                list.push_back(item);
    } else {
        PyErr_SetString(PyExc_TypeError, "In-place repeat only supported for list or string");
        return NULL;
    }

    Py_INCREF(self);
    return self;
}

static PySequenceMethods NodelObject_as_sequence = {
    .sq_length = NodelObject_sq_length,
    .sq_concat = NodelObject_sq_concat,
    .sq_repeat = NodelObject_sq_repeat,
    .sq_item = NodelObject_sq_item,
    .sq_ass_item = NodelObject_sq_ass_item,
    .sq_contains = NodelObject_sq_contains,
    .sq_inplace_concat = NodelObject_sq_inplace_concat,
    .sq_inplace_repeat = NodelObject_sq_inplace_repeat,
};


//-----------------------------------------------------------------------------
// NodelObject Methods
//-----------------------------------------------------------------------------

static PyMethodDef NodelObject_methods[] = {
    {NULL, NULL}
};

//-----------------------------------------------------------------------------
// Type slots
//-----------------------------------------------------------------------------

static PyObject* NodelObject_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    return type->tp_alloc(type, 0);
}

static int NodelObject_init(PyObject* self, PyObject* args, PyObject* kwds) {
    NodelObject* nd_self = (NodelObject*)self;
    std::construct_at<Object>(&nd_self->obj);

    PyObject* arg = NULL;  // borrowed reference
    if (!PyArg_ParseTuple(args, "|O", &arg))
        return -1;

    if (arg != NULL) {
        nd_self->obj = support.to_object(arg);
        if (PyErr_Occurred()) return -1;
    }

    return 0;
}

static void NodelObject_dealloc(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    std::destroy_at<Object>(&nd_self->obj);
    Py_TYPE(self)->tp_free(self);
}

static PyObject* NodelObject_str(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    return support.to_str_repr(nd_self->obj, false);
}

static PyObject* NodelObject_repr(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    return support.to_str_repr(nd_self->obj, true);
}

static PyObject* NodelObject_richcompare(PyObject* self, PyObject* other, int op) {
    NodelObject* nd_self = (NodelObject*)self;

    // iterator sentinel comparison
    if (self == nodel_sentinel) {
        if (other == nodel_sentinel) Py_RETURN_TRUE;
        Py_RETURN_FALSE;
    } else if (other == nodel_sentinel) {
        Py_RETURN_FALSE;
    }

    Object obj;
    if (NodelObject_CheckExact(other)) {
        obj = ((NodelObject*)other)->obj;
    } else {
        obj = support.to_object(other);
        if (PyErr_Occurred()) return NULL;
    }

    try {
        switch (op) {
            case Py_LT: if (nd_self->obj <  obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
            case Py_LE: if (nd_self->obj <= obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
            case Py_EQ: if (nd_self->obj == obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
            case Py_NE: if (nd_self->obj != obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
            case Py_GT: if (nd_self->obj >  obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
            case Py_GE: if (nd_self->obj >= obj) Py_RETURN_TRUE; else Py_RETURN_FALSE;
            default:    break;
        }
    } catch (const WrongType& ex) {
        PyErr_SetString(PyExc_TypeError, ex.what());
    }
    return NULL;
}

static Py_hash_t NodelObject_hash(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    auto& self_obj = nd_self->obj;

    try {
        Py_hash_t hash;
        if (support.hash(self_obj, &hash) == -1)
            return -1;
        if (hash == -1) hash = -2;
        return hash;
    } catch (const nodel::WrongType& ex) {
        PyErr_SetString(PyExc_TypeError, ex.what());
        return -1;
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return -1;
    }
}

static PyObject* NodelObject_getattro(PyObject* self, PyObject* name) {
    PyObject* val = PyObject_GenericGetAttr(self, name);
    if (val != NULL) return val;

    PyErr_Clear();

    NodelObject* nd_self = (NodelObject*)self;
    Object& self_obj = nd_self->obj;
    try {
        Object attr_value = self_obj.get(support.to_key(name));
        if (attr_value.is_nil()) Py_RETURN_NONE;
        return support.prepare_return_value(attr_value);
    } catch (const WrongType& ex) {
        python::raise_type_error(self_obj);
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
    }

    return NULL;
}

static int NodelObject_setattro(PyObject* self, PyObject* name, PyObject* val) {
    NodelObject* nd_self = (NodelObject*)self;
    Object& self_obj = nd_self->obj;
    auto nd_key = support.to_key(name);
    try {
        if (val == NULL) {
            self_obj.del(nd_key);
        } else if (NodelObject_CheckExact(val)) {
            self_obj.set(nd_key, ((NodelObject*)val)->obj);
        } else if (val == Py_None) {
            self_obj.set(nd_key, nil);
        } else {
            Object nd_val = support.to_object(val);
            if (nd_val == nil && PyErr_Occurred()) return -1;
            self_obj.set(nd_key, nd_val);
        }
        return 0;
    } catch (const WrongType& ex) {
        python::raise_type_error(self_obj);
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
    }

    return -1;
}

static PyObject* NodelObject_iter(PyObject* self) {
    NodelObject* nd_self = (NodelObject*)self;
    Object& self_obj = nd_self->obj;
    try {
        auto type = self_obj.type();
        if (type == Object::STR) {
            // TODO: optimize eliminate copy
            RefMgr py_str = python::to_str(self_obj.as<String>());
            return PyObject_GetIter(py_str.get());
        } else if (is_map(self_obj)) {
            return iter_keys(self, NULL);
        } else {
            return iter_values(self, NULL);
        }
    } catch (const std::exception& ex) {
        PyErr_SetString(PyExc_RuntimeError, ex.what());
        return NULL;
    }
}


//-----------------------------------------------------------------------------
// Type definition
//-----------------------------------------------------------------------------

constexpr auto NodelObject_doc = \
"Flyweight wrapper around the nodel::Object class.\n"
"Object instances can be created, or assigned, from any of the following Python\n"
"objects:\n"
"- None\n"
"- True|False\n"
"- int|float|str\n"
"- dict|list\n"
"- nodel.Object\n"
"Object instances can also be created by calling the module function, `bind`.\n"
"See `nodel.bind` for details.";

PyTypeObject NodelObjectType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name        = "nodel.Object",
    .tp_basicsize   = sizeof(NodelObject),
    .tp_dealloc     = (destructor)NodelObject_dealloc,
    .tp_repr        = (reprfunc)NodelObject_repr,
    .tp_as_number   = &NodelObject_as_number,
    .tp_as_mapping  = &NodelObject_as_mapping,
    .tp_as_sequence = &NodelObject_as_sequence,
    .tp_str         = (reprfunc)NodelObject_str,
    .tp_getattro    = &NodelObject_getattro,
    .tp_setattro    = &NodelObject_setattro,
    .tp_doc         = PyDoc_STR(NodelObject_doc),
    .tp_richcompare = &NodelObject_richcompare,
    .tp_hash        = (hashfunc)NodelObject_hash,
    .tp_iter        = &NodelObject_iter,
    .tp_methods     = NodelObject_methods,
    .tp_init        = NodelObject_init,
    .tp_new         = NodelObject_new,
};


//-----------------------------------------------------------------------------
// NodelObject creation
//-----------------------------------------------------------------------------

NodelObject* NodelObject_wrap(const Object& obj) {
    PyObject* module = PyState_FindModule(&nodel_module_def);
    if (module == NULL) return NULL;

    NodelObject* nd_obj = (NodelObject*)PyObject_CallMethodObjArgs(module, PyUnicode_InternFromString("Object"), NULL);
    if (nd_obj == NULL) return NULL;

    nd_obj->obj = obj;
    return nd_obj;
}

} // extern C

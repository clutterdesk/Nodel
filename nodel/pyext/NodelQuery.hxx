/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <Python.h>
#include <nodel/core/Query.hxx>

#include <memory>
#include <optional>

extern "C" {

extern PyTypeObject NodelQueryType;
extern PyTypeObject NodelQueryIterType;

#define NodelQuery_CheckExact(op) (Py_TYPE(op) == &NodelQueryType)

typedef struct {
    PyObject_HEAD
    std::shared_ptr<nodel::Query> query;
    PyObject* spec;
} NodelQuery;

typedef struct {
    PyObject_HEAD
    std::shared_ptr<nodel::Query> query;    // owns the Query referenced by eval
    std::optional<nodel::QueryEval> eval;
} NodelQueryIter;

extern PyObject* NodelQuery_from_spec(PyObject* spec);
extern PyObject* NodelQueryIter_new(const std::shared_ptr<nodel::Query>& query, const nodel::Object& obj);

} // extern C

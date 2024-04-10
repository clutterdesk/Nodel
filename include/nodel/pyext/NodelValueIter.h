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
#include <nodel/core/Object.h>

extern "C" {

extern PyTypeObject NodelValueIterType;

typedef struct {
    PyObject_HEAD
    nodel::ValueRange range;
    nodel::ValueIterator it;
    nodel::ValueIterator end;
} NodelValueIter;

} // extern C

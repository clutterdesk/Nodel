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
// limitations under the License.#pragma once
#pragma once

#include "Object.h"

namespace nodel {

template<class InRange, class UnaryPredicate>
Object find_first(InRange range, UnaryPredicate pred)
{
    auto it = range.begin();
    auto end = range.end();
    for (; it != end; ++it) {
        if (pred(*it))
            return *it;
    }
    return {};
}

} // namespace nodel

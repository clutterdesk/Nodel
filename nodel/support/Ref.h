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

#include <nodel/types.h>

namespace nodel {

template <class T>
class Ref
{
  public:
    Ref(T* ptr) : m_ptr(ptr) { assert(m_ptr != nullptr); inc_ref_count(); }
    ~Ref() { if (m_ptr != nullptr) dec_ref_count(); }

    Ref(const Ref& other) {
        m_ptr = other.m_ptr;
        inc_ref_count();
    }

    Ref(Ref&& other) {
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
    }

    Ref& operator = (const Ref& other) {
        if (m_ptr != other.m_ptr) {
            dec_ref_count();
            m_ptr = other.m_ptr;
            inc_ref_count();
        }
        return *this;
    }

    Ref& operator = (Ref&& other) {
        if (m_ptr != other.m_ptr) {
            dec_ref_count();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    const T& operator * () const { return *m_ptr; }
    T& operator * ()             { return *m_ptr; }

    const T* operator -> () const { return m_ptr; }
    T* operator -> ()             { return m_ptr; }

    const T* get() const          { return m_ptr; }
    T* get()                      { return m_ptr; }

    refcnt_t ref_count() const { return m_ptr->m_ref_count; }

  private:
    void inc_ref_count() { ++(m_ptr->m_ref_count); }
    void dec_ref_count() { if (--(m_ptr->m_ref_count) == 0) delete m_ptr; }

  private:
    T* m_ptr;
};

} // namespace nodel

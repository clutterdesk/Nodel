/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <nodel/support/types.hxx>
#include <nodel/support/exception.hxx>

namespace nodel {

template <class T>
class Ref
{
  public:
    Ref() : m_ptr{nullptr} {}
    Ref(T* ptr) : m_ptr(ptr) { inc_ref_count(); }
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
            if (m_ptr != nullptr)
                dec_ref_count();
            m_ptr = other.m_ptr;
            if (m_ptr != nullptr)
                inc_ref_count();
        }
        return *this;
    }

    Ref& operator = (Ref&& other) {
        if (m_ptr != other.m_ptr) {
            if (m_ptr != nullptr)
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

    const T* get() const { return m_ptr; }
    T* get()             { return m_ptr; }

    operator bool () const { return m_ptr != nullptr; }

    refcnt_t ref_count() const { return m_ptr->m_ref_count; }

  private:
    void inc_ref_count() { ++(m_ptr->m_ref_count); }
    void dec_ref_count() { if (--(m_ptr->m_ref_count) == 0) delete m_ptr; }

  private:
    T* m_ptr;
};

} // namespace nodel

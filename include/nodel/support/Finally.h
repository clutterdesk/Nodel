#pragma once

#include <functional>

namespace nodel {

class Finally
{
  public:
    template <typename Func>
    Finally(Func&& func) : m_func{std::forward<Func>(func)} {}
    ~Finally() { m_func(); }
  private:
    std::function<void()> m_func;
};

} // nodel namespace

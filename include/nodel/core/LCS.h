/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <iostream>
#include <vector>
#include <algorithm>

namespace nodel::algo {

template <class List>
class LCS
{
  public:
    using Result = std::vector<std::pair<int, int>>;

  public:
    size_t search(const List& lhs, const List& rhs, List* p_out = nullptr);

    Result::const_reverse_iterator begin() const { return m_lcs.crbegin(); }
    Result::const_reverse_iterator end() const   { return m_lcs.crend(); }

  protected:
    void build_matrix(const List& lhs, const List& rhs);
    void backtrack(const List& lhs, const List& rhs);

  private:
    std::vector<int> m_cmp;
    Result m_lcs;
};


template <class List>
size_t LCS<List>::search(const List& lhs, const List& rhs, List* p_out) {
    build_matrix(lhs, rhs);
    backtrack(lhs, rhs);

    if (p_out != nullptr) {
        auto &out = *p_out;
        for (auto& ix_pair : *this)
            out.push_back(lhs[ix_pair.first]);
    }

    return m_lcs.size();
}

template <class List>
void LCS<List>::build_matrix(const List& lhs, const List& rhs) {
    m_cmp.clear();
    size_t l_n = lhs.size() + 1;
    size_t r_n = rhs.size() + 1;
    m_cmp.resize(2 * r_n);

    // (l_ix % 2) * r_n + r_ix  (index into m_cmp for (l_ix, r_ix))
    // ((l_ix - 1) % 2) * r_n + r_ix  (index into m_cmp for prevous row)

    size_t i = r_n;  // l_ix % 2
    size_t j = 0;    // (l_ix - 1) % 2
    for (size_t l_ix = 1; l_ix < l_n; ++l_ix) {
        for (size_t r_ix = 1; r_ix < r_n; ++r_ix) {
            if (lhs[l_ix - 1] == rhs[r_ix - 1]) {
                m_cmp[i + r_ix] = m_cmp[j + r_ix - 1] + 1;
            } else {
                m_cmp[i + r_ix] = std::max(m_cmp[j + r_ix], m_cmp[i + r_ix - 1]);
            }
        }
        std::swap(i, j);
    }
}

template <class List>
void LCS<List>::backtrack(const List& lhs, const List& rhs) {
    m_lcs.clear();
    auto l_n = lhs.size() + 1;
    auto r_n = rhs.size() + 1;
    int l_ix = l_n - 1;
    int r_ix = r_n - 1;
    while (l_ix > 0 && r_ix > 0) {
        auto curr = (l_ix % 2) * r_n + r_ix;
        auto up = ((l_ix - 1) % 2) * r_n + r_ix;
        auto left = curr - 1;
        if (lhs[l_ix - 1] == rhs[r_ix - 1]) {
            m_lcs.push_back(std::make_pair(l_ix - 1, r_ix - 1));
            --l_ix;
            --r_ix;
        } else if (m_cmp[up] > m_cmp[left]) {
            --l_ix;
        } else {
            --r_ix;
        }
    }
}

} // namespace nodel::algo

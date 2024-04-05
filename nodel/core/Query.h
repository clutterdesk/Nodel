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

#include "Key.h"
#include "Object.h"
#include "algo.h"

#include <deque>


namespace nodel {

class QueryEval;

namespace impl {

class StackItem
{
  public:
    enum { OBJECT, ITER };

    struct IteratorBundle
    {
        IteratorBundle() {}
        IteratorBundle(ValueRange&& range) : m_range{std::forward<ValueRange>(range)}, m_it{m_range.begin()}, m_end{m_range.end()} {}

        auto& operator = (IteratorBundle&& other) {
            m_range = std::move(other.m_range);
            m_it = m_range.begin();
            m_end = m_range.end();
            return *this;
        }

        ValueRange m_range;
        ValueIterator m_it;
        ValueIterator m_end;
    };

    union ReprIX
    {
        ReprIX() {}
        ReprIX(const Object& obj)  : m_obj{obj} {}
        ReprIX(ValueRange&& range) : m_bundle{std::forward<ValueRange>(range)} {}
        ~ReprIX() {}

        Object m_obj;             // has default ctor
        IteratorBundle m_bundle;  // does not have default ctor
    };

  public:
    StackItem(uint32_t step_i, const Object& obj) : m_repr{obj}, m_step_i{step_i}, m_repr_ix{OBJECT} {}
    StackItem(uint32_t step_i, ValueRange&& range) : m_repr{std::forward<ValueRange>(range)}, m_step_i{step_i}, m_repr_ix{ITER} {}
    StackItem(StackItem&&);

    Object peek();
    Object next();
    bool done();

  private:
    ReprIX m_repr;
    uint32_t m_step_i;
    uint8_t m_repr_ix;

  friend class ::nodel::QueryEval;
};

} // impl namespace


struct Step
{
    enum class Axis {
        ANCESTOR,  // TODO: rename "LINEAGE"?
        PARENT,
        SELF,
        CHILD,
        SUBTREE
    };

//    enum class Push {
//        FRONT,
//        BACK
//    };

    Step(Axis axis)                 : m_axis{axis}, m_key{nil} {}
    Step(Axis axis, const Key& key) : m_axis{axis}, m_key{key} {}

    Axis m_axis;
    Key m_key;
//    Push m_push;
};


using namespace nodel::impl;

class QueryEval;

class Query
{
  public:
    Query() = default;
    Query(const Step& step);

    template <class ... Args>
    Query(const Step& step, Args&& ... steps);

    void add_steps(const Step& step);

    template <class ... Args>
    void add_steps(const Step& step, Args&& ... steps);

    QueryEval eval(const Object& obj) const;
    QueryEval eval(ValueRange&& range) const;

  private:
    std::vector<Step> m_steps;

  friend class QueryEval;
};


class QueryEval
{
  public:
    QueryEval(const Query& query, const Object& obj);
    QueryEval(const Query& query, ValueRange&& range);

    Object next();

  private:
    const Query& m_query;
    std::deque<StackItem> m_fifo;
};


inline
StackItem::StackItem(StackItem&& other) : m_step_i{other.m_step_i}, m_repr_ix{other.m_repr_ix} {
    if (other.m_repr_ix == OBJECT) {
        m_repr.m_obj = other.m_repr.m_obj;
    } else {
        m_repr.m_bundle = IteratorBundle{std::move(other.m_repr.m_bundle.m_range)};
    }
}

inline
Object StackItem::peek() {
    if (m_repr_ix == OBJECT) {
        return m_repr.m_obj;
    } else {
        auto& it = m_repr.m_bundle.m_it;
        if (it != m_repr.m_bundle.m_end)
            return *it;
    }
    return nil;
}

inline
Object StackItem::next() {
    if (m_repr_ix == OBJECT) {
        Object next_obj = m_repr.m_obj;
        m_repr.m_obj = nil;
        return next_obj;
    } else {
        auto& it = m_repr.m_bundle.m_it;
        if (it != m_repr.m_bundle.m_end) {
            Object next_obj = *it;
            ++it;
            return next_obj;
        }
        return nil;
    }
}

inline
bool StackItem::done() {
    return (m_repr_ix == ITER)? (m_repr.m_bundle.m_it == m_repr.m_bundle.m_end): true;
}


inline
Query::Query(const Step& step) {
    add_steps(step);
}

template <class ... Args>
Query::Query(const Step& step, Args&& ... steps) {
    add_steps(step);
    add_steps(std::forward<Args>(steps) ...);
}


inline
void Query::add_steps(const Step& step) {
    m_steps.push_back(step);
}

template <class ... Args>
void Query::add_steps(const Step& step, Args&& ... steps) {
    add_steps(step);
    add_steps(std::forward<Args>(steps) ...);
}


inline
QueryEval Query::eval(const Object& obj) const {
    return {*this, obj};
}

inline
QueryEval Query::eval(ValueRange&& range) const {
    return {*this, std::forward<ValueRange>(range)};
}


inline
QueryEval::QueryEval(const Query& query, const Object& obj) : m_query{query} {
    m_fifo.push_front({0UL, obj});
}

inline
QueryEval::QueryEval(const Query& query, ValueRange&& range) : m_query{query} {
    m_fifo.push_front({0UL, std::forward<ValueRange>(range)});
}

inline
Object QueryEval::next() {
    while (!m_fifo.empty()) {
        auto& item = m_fifo.front();
        auto curr_step_i = item.m_step_i;
        auto next_step_i = curr_step_i + 1;
        auto curr_obj = item.next();

        if (curr_step_i == m_query.m_steps.size()) {
            m_fifo.pop_front();
            if (curr_obj != nil) return curr_obj;
            continue;
        }

        auto& step = m_query.m_steps[curr_step_i];
        auto step_key =  step.m_key;
        auto step_axis = step.m_axis;
//        auto step_push = step.m_push;

        if (item.m_repr_ix != StackItem::ITER || item.done()) {
            m_fifo.pop_front();
        }

        switch (step_axis) {
            case Step::Axis::ANCESTOR: {
                const auto& next_obj = curr_obj.parent();
                if (next_obj != nil) {
                    // continue current step
                    m_fifo.push_front({curr_step_i, next_obj});
                }

                // proceed to next step, or return obj if last step
                if (step_key == nil || step_key == curr_obj.key()) {
                    if (next_step_i < m_query.m_steps.size()) {
                        m_fifo.push_front({next_step_i, curr_obj});
                    } else {
                        return curr_obj;
                    }
                }

                break;
            }

            case Step::Axis::PARENT: {
                const auto& next_obj = curr_obj.parent();
                if (next_obj != nil) {
                    if (step_key == nil || step_key == next_obj.key()) {
                        // proceed to next step, or return obj if last step
                        if (next_step_i < m_query.m_steps.size()) {
                            m_fifo.push_front({next_step_i, next_obj});
                        } else if (step_key == nil || step_key == next_obj.key()) {
                            return next_obj;
                        }
                    }
                }
                break;
            }

            case Step::Axis::SELF: {
                if (step_key == nil || step_key == curr_obj.key()) {
                    // proceed to next step, or return obj if last step
                    if (next_step_i < m_query.m_steps.size()) {
                        m_fifo.push_front({next_step_i, curr_obj});
                    } else {
                        return curr_obj;
                    }
                }
                break;
            }

            case Step::Axis::CHILD: {
                if (curr_obj.is_container()) {
                    if (step_key == nil) {
                        // NOTE: size of sparse data-source may be unknown, so always create iterator
                        m_fifo.push_front({next_step_i, curr_obj.iter_values()});
                        if (m_fifo.front().done()) m_fifo.pop_front();
                    } else {
                        const auto& next_obj = curr_obj.get(step_key);
                        if (next_obj != nil) {
                            // proceed to next step, or return obj if last step
                            if (next_step_i < m_query.m_steps.size()) {
                                m_fifo.push_front({next_step_i, next_obj});
                            } else {
                                return next_obj;
                            }
                        }
                    }
                }
                break;
            }

            case Step::Axis::SUBTREE: {
                // continue current step: push current object children
                if (curr_obj.is_container()) {
                    // NOTE: size of sparse data-source may be unknown, so always create iterator
                    m_fifo.push_front({curr_step_i, curr_obj.iter_values()});
                    if (m_fifo.front().done()) m_fifo.pop_front();
                }

                if (step_key == nil || step_key == curr_obj.key()) {
                    // proceed to next step, or return obj if last step
                    if (next_step_i < m_query.m_steps.size()) {
                        m_fifo.push_front({next_step_i, curr_obj});
                    } else {
                        return curr_obj;
                    }
                }

                break;
            }
        }
    }

    return nil;
}

} // nodel namespace

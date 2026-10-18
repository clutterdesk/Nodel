/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include "Key.hxx"
#include "Object.hxx"

#include <nodel/parser/json.hxx>

#include <deque>
#include <memory>
#include <optional>


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

        IteratorBundle(ValueRange&& range)
          : m_range{std::forward<ValueRange>(range)}, m_it{m_range.begin()}, m_end{m_range.end()} {}

        IteratorBundle(ValueRange&& range, Predicate pred)
          : m_range{std::forward<ValueRange>(range)}, m_it{m_range.begin()}, m_end{m_range.end()}, m_pred{std::move(pred)} {}

        auto& operator = (IteratorBundle&& other) {
            m_range = std::move(other.m_range);
            m_it = m_range.begin();
            m_end = m_range.end();
            m_pred = std::move(other.m_pred);
            m_matched = false;
            return *this;
        }

        // Advance past values that do not satisfy the predicate.
        void skip() {
            if (m_matched || !m_pred) return;
            while (m_it != m_end && !m_pred(*m_it)) ++m_it;
            m_matched = true;
        }

        void advance() {
            ++m_it;
            m_matched = false;
        }

        ValueRange m_range;
        ValueIterator m_it;
        ValueIterator m_end;
        Predicate m_pred;
        bool m_matched = false;  // true, if m_it is known to satisfy m_pred
    };

    union ReprIX
    {
        ReprIX() {}  // used by StackItem(StackItem&&)
        ReprIX(const Object& obj)  : m_obj{obj} {}
        ReprIX(ValueRange&& range) : m_bundle{std::forward<ValueRange>(range)} {}
        ReprIX(ValueRange&& range, Predicate pred) : m_bundle{std::forward<ValueRange>(range), std::move(pred)} {}
        ~ReprIX() {}

        Object m_obj;             // has default ctor
        IteratorBundle m_bundle;  // does not have default ctor
    };

  public:
    StackItem(uint32_t step_i, const Object& obj) : m_repr{obj}, m_step_i{step_i}, m_repr_ix{OBJECT} {}
    StackItem(uint32_t step_i, ValueRange&& range) : m_repr{std::forward<ValueRange>(range)}, m_step_i{step_i}, m_repr_ix{ITER} {}
    StackItem(uint32_t step_i, ValueRange&& range, Predicate pred)
      : m_repr{std::forward<ValueRange>(range), std::move(pred)}, m_step_i{step_i}, m_repr_ix{ITER} {}
    StackItem(StackItem&&);
    ~StackItem();

    Object peek();
    Object next();
    bool done();

  private:
    ReprIX m_repr;
    uint32_t m_step_i;
    uint8_t m_repr_ix;

  friend class ::nodel::QueryEval;
};

// Like Object::get, but returns nil instead of throwing when a list is
// subscripted with a non-integer key.
inline
Object get_child(const Object& obj, const Key& key) {
    if (obj.type() == Object::LIST && !key.is_any_int()) return nil;
    return obj.get(key);
}

class QueryParser;

} // impl namespace


enum class Axis {
    ANCESTOR,  // TODO: rename "LINEAGE"?
    PARENT,
    SELF,
    CHILD,
    SUBTREE
};


struct Step
{
    Step(Axis axis)                 : m_axis{axis}, m_key{nil} {}
    Step(Axis axis, const Key& key) : m_axis{axis}, m_key{key} {}

    template <typename Func>
    Step(Axis axis, const Key& key, Func&& pred) : m_axis{axis}, m_key{key}, m_pred{std::forward<Func>(pred)} {}

    Axis m_axis;
    Key m_key;
    Predicate m_pred;
};


using namespace nodel::impl;

class QueryEval;

/// A query that yields the Objects reachable from a starting Object by a
/// sequence of steps.
/// - A query can be built from Step instances, or parsed from a string
///   specification (see Query(const StringView&)).
class Query
{
  public:
    Query() = default;
    Query(const Step& step);
    Query(const StringView& spec);

    template <class ... Args>
    Query(const Step& step, Args&& ... steps);

    void add_steps(const Step& step);

    template <class ... Args>
    void add_steps(const Step& step, Args&& ... steps);

    Query& operator += (const Query& other) {
        for (const auto& step : other.m_steps)
            m_steps.push_back(step);
        return *this;
    }

    Query operator + (const Query& other) {
        Query query = *this;
        query += other;
        return query;
    }

    Query& operator += (const Step& step) {
        add_steps(step);
        return *this;
    }

    QueryEval iter_eval(const Object& obj) const;
    QueryEval iter_eval(ValueRange&& range) const;

    void eval(ObjectList& out, const Object&) const;
    void eval(ObjectList& out, ValueRange&&) const;

    ObjectList eval(const Object& obj) const  { ObjectList result; eval(result, obj); return result; }
    ObjectList eval(ValueRange&& range) const { ObjectList result; eval(result, std::forward<ValueRange>(range)); return result; }

    template <typename Predicate>
    void eval(ObjectList& out, const Object& obj, Predicate&& pred) const;

    template <typename Predicate>
    void eval(ObjectList& out, ValueRange&& range, Predicate&& pred) const;

    template <typename Predicate>
    ObjectList eval(const Object& obj, Predicate&& pred) const {
        ObjectList result;
        eval(result, obj, std::forward<Predicate>(pred));
        return result;
    }

    template <typename Predicate>
    ObjectList eval(ValueRange&& range, Predicate&& pred) const {
        ObjectList result;
        eval(result, std::forward<ValueRange>(range), std::forward<Predicate>(pred));
        return result;
    }

  private:
    std::vector<Step> m_steps;

  friend class QueryEval;
  friend class impl::QueryParser;
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
        std::construct_at(&m_repr.m_obj, std::move(other.m_repr.m_obj));
    } else {
        std::construct_at(&m_repr.m_bundle, std::move(other.m_repr.m_bundle.m_range), std::move(other.m_repr.m_bundle.m_pred));
    }
}

inline
StackItem::~StackItem() {
    switch (m_repr_ix) {
        case OBJECT: std::destroy_at(&m_repr.m_obj); break;
        case ITER:   std::destroy_at(&m_repr.m_bundle); break;
        default:     break;
    }
}

inline
Object StackItem::peek() {
    if (m_repr_ix == OBJECT) {
        return m_repr.m_obj;
    } else {
        auto& bundle = m_repr.m_bundle;
        bundle.skip();
        if (bundle.m_it != bundle.m_end)
            return *bundle.m_it;
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
        auto& bundle = m_repr.m_bundle;
        bundle.skip();
        if (bundle.m_it != bundle.m_end) {
            Object next_obj = *bundle.m_it;
            bundle.advance();
            return next_obj;
        }
        return nil;
    }
}

inline
bool StackItem::done() {
    if (m_repr_ix != ITER) return true;
    auto& bundle = m_repr.m_bundle;
    bundle.skip();
    return bundle.m_it == bundle.m_end;
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
QueryEval Query::iter_eval(const Object& obj) const {
    return {*this, obj};
}

inline
QueryEval Query::iter_eval(ValueRange&& range) const {
    return {*this, std::forward<ValueRange>(range)};
}

inline
void Query::eval(ObjectList& out, const Object& obj) const {
    auto it = iter_eval(obj);
    auto cur = it.next();
    while (cur != nil) {
        out.push_back(cur);
        cur = it.next();
    }
}

inline
void Query::eval(ObjectList& out, ValueRange&& range) const {
    auto it = iter_eval(std::forward<ValueRange>(range));
    auto cur = it.next();
    while (cur != nil) {
        out.push_back(cur);
        cur = it.next();
    }
}

template <typename Predicate>
void Query::eval(ObjectList& out, const Object& obj, Predicate&& pred) const {
    auto it = iter_eval(obj);
    for (auto cur = it.next(); cur != nil; cur = it.next())
        if (pred(cur))
            out.push_back(cur);
}

template <typename Predicate>
void Query::eval(ObjectList& out, ValueRange&& range, Predicate&& pred) const {
    auto it = iter_eval(std::forward<ValueRange>(range));
    for (auto cur = it.next(); cur != nil; cur = it.next())
        if (pred(cur))
            out.push_back(cur);
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
            if (item.m_repr_ix != StackItem::ITER || item.done()) m_fifo.pop_front();
            if (curr_obj != nil) return curr_obj;
            continue;
        }

        auto& step = m_query.m_steps[curr_step_i];
        auto step_key =  step.m_key;
        auto step_axis = step.m_axis;

        if (item.m_repr_ix != StackItem::ITER || item.done()) {
            m_fifo.pop_front();
        }

        switch (step_axis) {
            case Axis::ANCESTOR: {
                const auto& next_obj = curr_obj.parent();
                if (next_obj != nil) {
                    // continue current step
                    m_fifo.push_front({curr_step_i, next_obj});
                }

                // proceed to next step, or return obj if last step
                if ((step_key == nil || step_key == curr_obj.key()) && (!step.m_pred || step.m_pred(curr_obj))) {
                    if (next_step_i < m_query.m_steps.size()) {
                        m_fifo.push_front({next_step_i, curr_obj});
                    } else {
                        return curr_obj;
                    }
                }

                break;
            }

            case Axis::PARENT: {
                const auto& next_obj = curr_obj.parent();
                if (next_obj != nil && (!step.m_pred || step.m_pred(next_obj))) {
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

            case Axis::SELF: {
                if ((step_key == nil || step_key == curr_obj.key()) && (!step.m_pred || step.m_pred(curr_obj))) {
                    // proceed to next step, or return obj if last step
                    if (next_step_i < m_query.m_steps.size()) {
                        m_fifo.push_front({next_step_i, curr_obj});
                    } else {
                        return curr_obj;
                    }
                }
                break;
            }

            case Axis::CHILD: {
                if (nodel::is_container(curr_obj)) {
                    if (step_key == nil) {
                        // NOTE: size of sparse data-source may be unknown, so always create iterator
                        m_fifo.push_front({next_step_i, curr_obj.iter_values(), step.m_pred});
                        if (m_fifo.front().done()) m_fifo.pop_front();
                    } else {
                        const auto& next_obj = get_child(curr_obj, step_key);
                        if (next_obj != nil && (!step.m_pred || step.m_pred(next_obj))) {
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

            case Axis::SUBTREE: {
                // continue current step: push current object children
                // NOTE: the predicate filters yielded objects, not the descent
                if (nodel::is_container(curr_obj)) {
                    // NOTE: size of sparse data-source may be unknown, so always create iterator
                    m_fifo.push_front({curr_step_i, curr_obj.iter_values()});
                    if (m_fifo.front().done()) m_fifo.pop_front();
                }

                if ((step_key == nil || step_key == curr_obj.key()) && (!step.m_pred || step.m_pred(curr_obj))) {
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


namespace impl {

//////////////////////////////////////////////////////////////////////////////
/// Parser for the query mini-language.
/// The syntax extends OPath with wildcards and predicates:
/// - `x.y`, `x[0]`, `x["y z"]` - child steps, as in OPath
/// - `*`, `[*]`                - any child
/// - `**`                      - any descendant, including the current object
/// - `(pred)`                  - the children of the preceding result that
///                               satisfy `pred`, so `x(id=2)` yields the
///                               elements of `x` whose `id` is 2.
///
/// Predicate grammar:
/// @code
/// pred     := and_expr ('or' and_expr)*
/// and_expr := not_expr ('and' not_expr)*
/// not_expr := 'not' not_expr | '(' pred ')' | test
/// test     := operand (cmp_op operand)*    // chained: a < b < c
///           | operand 'not'? 'in' operand
/// cmp_op   := '=' | '==' | '!=' | '<' | '<=' | '>' | '>='
/// operand  := literal | '.' | query        // '.' is the object being tested
/// literal  := number | 'str' | "str" | [list] | true | false | nil | null
/// @endcode
///
/// A query operand is evaluated relative to the object being tested and may
/// yield zero or more values.  A comparison is true if any combination of
/// operand values satisfies it, so a comparison with a missing value is false.
/// Values that cannot be ordered (for example, a string and a number) do not
/// satisfy any ordering comparison.  A lone operand is true if it yields a
/// value other than `false`.  The right operand of `in` may be a list, whose
/// elements are tested for equality, or any other value.
//////////////////////////////////////////////////////////////////////////////
class QueryParser
{
  public:
    QueryParser(const StringView& spec) : m_spec{spec}, m_it{spec.cbegin()} {}

    Query parse();

  private:
    enum class Cmp { EQ, NE, LT, LE, GT, GE };

    struct Operand
    {
        enum Kind { LITERAL, SELF, PATH, QUERY };

        template <typename Func>
        bool any(const Object& target, Func&& func) const;

        Kind m_kind = LITERAL;
        Object m_literal;
        KeyList m_keys;
        std::shared_ptr<const Query> m_query;
    };

    void parse_steps(Query& query);
    Key parse_index();
    Predicate parse_or();
    Predicate parse_and();
    Predicate parse_not();
    Predicate parse_test();
    Operand parse_operand(bool is_in_rhs);
    Object parse_literal();
    std::optional<Cmp> parse_cmp();
    bool parse_word(const char* word);

    static bool compare(const Object& lhs, Cmp cmp, const Object& rhs);
    static bool contains(const Object& container, const Object& value);
    static bool is_key_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

    char peek() const    { return m_it == m_spec.cend()? '\0': *m_it; }
    char peek_next() const { return (m_it == m_spec.cend() || m_it + 1 == m_spec.cend())? '\0': *(m_it + 1); }
    bool consume(char c) { if (peek() != c) return false; ++m_it; return true; }
    void consume_whitespace() { OPath::consume_whitespace(m_spec, m_it); }

    [[noreturn]] void error(const char* message) const {
        throw parse::SyntaxError{m_spec, m_it - m_spec.cbegin(), message};
    }

  private:
    StringView m_spec;
    StringView::const_iterator m_it;
};

template <typename Func>
bool QueryParser::Operand::any(const Object& target, Func&& func) const {
    switch (m_kind) {
        case LITERAL: return func(m_literal);
        case SELF:    return func(target);
        case PATH: {
            Object obj = target;
            for (const auto& key : m_keys) {
                if (!nodel::is_container(obj)) return false;
                obj = get_child(obj, key);
            }
            return obj != nil && func(obj);
        }
        case QUERY: {
            auto it = m_query->iter_eval(target);
            for (auto obj = it.next(); obj != nil; obj = it.next())
                if (func(obj)) return true;
            return false;
        }
    }
    return false;
}

inline
Query QueryParser::parse() {
    Query query;
    consume_whitespace();
    parse_steps(query);
    consume_whitespace();
    if (m_it != m_spec.cend()) error("Unexpected character:");
    return query;
}

/// Parse a sequence of steps, stopping at the first character that cannot
/// continue the query.
inline
void QueryParser::parse_steps(Query& query) {
    // first step does not require a leading '.'
    char c = peek();
    if (is_key_char(c)) {
        query.add_steps({Axis::CHILD, OPath::parse_dot_key(m_spec, m_it)});
    } else if (c == '*') {
        ++m_it;
        query.add_steps(consume('*')? Step{Axis::SUBTREE}: Step{Axis::CHILD});
    } else if (c == '.' && !is_key_char(peek_next()) && peek_next() != '*') {
        ++m_it;  // '.' is the starting object, as in '.', '.[0]' or '.(id=2)'
    }

    while (true) {
        c = peek();
        if (c == '.') {
            ++m_it;
            c = peek();
            if (is_key_char(c)) {
                query.add_steps({Axis::CHILD, OPath::parse_dot_key(m_spec, m_it)});
            } else if (c == '*') {
                ++m_it;
                query.add_steps(consume('*')? Step{Axis::SUBTREE}: Step{Axis::CHILD});
            } else {
                error("Expected key, '*' or '**':");
            }
        } else if (c == '[') {
            ++m_it;
            consume_whitespace();
            if (consume('*')) {
                consume_whitespace();
                if (!consume(']')) error("Missing closing ']':");
                query.add_steps({Axis::CHILD});
            } else {
                query.add_steps({Axis::CHILD, parse_index()});
            }
        } else if (c == '(') {
            ++m_it;
            auto pred = parse_or();
            consume_whitespace();
            if (!consume(')')) error("Expected ')':");
            query.add_steps({Axis::CHILD, nil, std::move(pred)});
        } else {
            break;
        }
    }
}

/// Parse a subscript following '[', and the closing ']'.
inline
Key QueryParser::parse_index() {
    char c = peek();
    if (c != '\'' && c != '"') {
        auto it = m_it;
        if (it != m_spec.cend() && *it == '-') ++it;
        auto digits = it;
        while (it != m_spec.cend() && std::isdigit(static_cast<unsigned char>(*it))) ++it;
        auto end = it;
        while (it != m_spec.cend() && std::isspace(static_cast<unsigned char>(*it))) ++it;
        if (digits == end || it == m_spec.cend() || *it != ']')
            error("Expected integer, quoted key or '*':");
        Key key = str_to_int(StringView{m_it, end});
        m_it = it + 1;
        return key;
    }
    return OPath::parse_brace_key(m_spec, m_it);
}

inline
Predicate QueryParser::parse_or() {
    auto pred = parse_and();
    while (true) {
        consume_whitespace();
        if (!parse_word("or")) break;
        auto rhs = parse_and();
        pred = [lhs = std::move(pred), rhs = std::move(rhs)] (const Object& obj) { return lhs(obj) || rhs(obj); };
    }
    return pred;
}

inline
Predicate QueryParser::parse_and() {
    auto pred = parse_not();
    while (true) {
        consume_whitespace();
        if (!parse_word("and")) break;
        auto rhs = parse_not();
        pred = [lhs = std::move(pred), rhs = std::move(rhs)] (const Object& obj) { return lhs(obj) && rhs(obj); };
    }
    return pred;
}

inline
Predicate QueryParser::parse_not() {
    consume_whitespace();
    if (parse_word("not")) {
        auto pred = parse_not();
        return [pred = std::move(pred)] (const Object& obj) { return !pred(obj); };
    }
    if (consume('(')) {
        auto pred = parse_or();
        consume_whitespace();
        if (!consume(')')) error("Expected ')':");
        return pred;
    }
    return parse_test();
}

inline
Predicate QueryParser::parse_test() {
    auto first = parse_operand(false);
    consume_whitespace();

    // membership
    auto save_it = m_it;
    bool negate = false;
    if (parse_word("not")) {
        consume_whitespace();
        if (parse_word("in")) negate = true; else m_it = save_it;
    }
    if (negate || parse_word("in")) {
        consume_whitespace();
        auto rhs = parse_operand(true);
        return [lhs = std::move(first), rhs = std::move(rhs), negate] (const Object& obj) {
            bool found = lhs.any(obj, [&rhs, &obj] (const Object& l) {
                return rhs.any(obj, [&l] (const Object& r) { return contains(r, l); });
            });
            return found != negate;
        };
    }

    // comparison chain
    std::vector<Operand> operands;
    std::vector<Cmp> cmps;
    operands.push_back(std::move(first));
    for (auto cmp = parse_cmp(); cmp.has_value(); cmp = parse_cmp()) {
        consume_whitespace();
        cmps.push_back(*cmp);
        operands.push_back(parse_operand(false));
        consume_whitespace();
    }

    if (cmps.size() == 0) {
        return [opnd = std::move(operands[0])] (const Object& obj) {
            return opnd.any(obj, [] (const Object& value) {
                return !(value.type() == Object::BOOL && !value.as<bool>());
            });
        };
    }

    return [operands = std::move(operands), cmps = std::move(cmps)] (const Object& obj) {
        for (size_t i = 0; i < cmps.size(); ++i) {
            auto& rhs = operands[i + 1];
            auto cmp = cmps[i];
            bool result = operands[i].any(obj, [&rhs, &obj, cmp] (const Object& l) {
                return rhs.any(obj, [&l, cmp] (const Object& r) { return compare(l, cmp, r); });
            });
            if (!result) return false;
        }
        return true;
    };
}

inline
QueryParser::Operand QueryParser::parse_operand(bool is_in_rhs) {
    Operand operand;
    char c = peek();
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '\'' || c == '"' || (c == '[' && is_in_rhs)) {
        operand.m_literal = parse_literal();
    } else if (parse_word("true")) {
        operand.m_literal = true;
    } else if (parse_word("false")) {
        operand.m_literal = false;
    } else if (parse_word("nil") || parse_word("null")) {
        operand.m_literal = nil;
    } else if (c == '.' && !is_key_char(peek_next()) && peek_next() != '*' && peek_next() != '[' && peek_next() != '(') {
        ++m_it;
        operand.m_kind = Operand::SELF;
    } else if (is_key_char(c) || c == '*' || c == '[' || c == '.') {
        for (auto word : {"and", "or", "not", "in"}) {
            auto save_it = m_it;
            if (parse_word(word)) {
                m_it = save_it;
                error("Expected operand:");
            }
        }

        auto query = std::make_shared<Query>();
        parse_steps(*query);

        bool is_path = true;
        for (const auto& step : query->m_steps)
            is_path &= step.m_axis == Axis::CHILD && step.m_key != nil && !step.m_pred;

        if (is_path) {
            operand.m_kind = Operand::PATH;
            for (const auto& step : query->m_steps)
                operand.m_keys.push_back(step.m_key);
        } else {
            operand.m_kind = Operand::QUERY;
            operand.m_query = std::move(query);
        }
    } else {
        error("Expected operand:");
    }
    return operand;
}

inline
Object QueryParser::parse_literal() {
    json::impl::Parser parser{parse::StringStreamAdapter{StringView{m_it, m_spec.cend()}}};
    if (!parser.parse_any()) error("Invalid literal:");
    m_it += parser.m_it.consumed();
    return parser.m_curr;
}

inline
std::optional<QueryParser::Cmp> QueryParser::parse_cmp() {
    consume_whitespace();
    switch (peek()) {
        case '=': ++m_it; consume('='); return Cmp::EQ;
        case '!':
            if (peek_next() != '=') return std::nullopt;
            m_it += 2;
            return Cmp::NE;
        case '<': ++m_it; return consume('=')? Cmp::LE: Cmp::LT;
        case '>': ++m_it; return consume('=')? Cmp::GE: Cmp::GT;
        default:  return std::nullopt;
    }
}

/// Consume a keyword, if it is next and is not the prefix of a longer word.
inline
bool QueryParser::parse_word(const char* word) {
    auto it = m_it;
    for (; *word != 0; ++word, ++it)
        if (it == m_spec.cend() || *it != *word) return false;
    if (it != m_spec.cend() && is_key_char(*it)) return false;
    m_it = it;
    return true;
}

inline
bool QueryParser::compare(const Object& lhs, Cmp cmp, const Object& rhs) {
    // Object comparison operators resolve a DataSource on the left operand
    if (has_data_source(rhs) && !has_data_source(lhs)) {
        switch (cmp) {
            case Cmp::LT: return compare(rhs, Cmp::GT, lhs);
            case Cmp::LE: return compare(rhs, Cmp::GE, lhs);
            case Cmp::GT: return compare(rhs, Cmp::LT, lhs);
            case Cmp::GE: return compare(rhs, Cmp::LE, lhs);
            default:      return compare(rhs, cmp, lhs);
        }
    }

    if (cmp == Cmp::EQ) return lhs == rhs;
    if (cmp == Cmp::NE) return !(lhs == rhs);

    auto l_type = lhs.type();
    auto r_type = rhs.type();
    bool is_ordered = (is_number(lhs) && is_number(rhs)) ||
                      (l_type == Object::STR && r_type == Object::STR) ||
                      (l_type == Object::BOOL && r_type == Object::BOOL);
    if (!is_ordered) return false;

    auto order = lhs <=> rhs;
    switch (cmp) {
        case Cmp::LT: return order < 0;
        case Cmp::LE: return order <= 0;
        case Cmp::GT: return order > 0;
        case Cmp::GE: return order >= 0;
        default:      return false;
    }
}

/// Returns true if `value` is an element of the list, `container`, or if
/// `container` is not a list and is equal to `value`.
inline
bool QueryParser::contains(const Object& container, const Object& value) {
    if (container.type() != Object::LIST) return compare(value, Cmp::EQ, container);
    for (const auto& elem : container.iter_values())
        if (compare(value, Cmp::EQ, elem)) return true;
    return false;
}

} // impl namespace


/// Construct a query from a string specification.
/// @param spec The specification, whose syntax is described by impl::QueryParser.
/// @throws parse::SyntaxError if the specification is malformed.
inline
Query::Query(const StringView& spec) {
    *this = impl::QueryParser{spec}.parse();
}

inline
Query operator ""_query (const char* str, size_t size) {
    return Query{StringView{str, size}};
}

} // nodel namespace

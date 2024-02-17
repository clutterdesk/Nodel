#pragma once

#include <ctype.h>
#include <cstdlib>
#include <cerrno>
#include <sstream>

#include <fmt/core.h>

#include "Object.h"
#include "except.h"

namespace nodel {
namespace json {
namespace impl {

template <typename StreamType>
struct Parser
{
  private:
    struct StreamIterator
    {
        StreamIterator(StreamType&& stream) : stream{std::move(stream)} {}

        StreamIterator(StreamIterator&&) = default;
        StreamIterator(const StreamIterator&) = delete;
        auto operator = (StreamIterator&&) = delete;
        auto operator = (StreamIterator&) = delete;

        auto operator * () { return stream.peek(); }
        auto& operator ++ () { stream.get(); pos++; return *this; }
        auto& operator ++ (int) { stream.get(); pos++; return *this; }
        size_t consumed() const { return pos; }
        bool done() const { return stream.eof(); }
        bool error() const { return stream.fail(); }
        StreamType stream;
        size_t pos = 0;
    };

  public:
    Parser(StreamType&& stream) : it{std::move(stream)} {}

    Parser(Parser&&) =  default;
    Parser(const Parser&) = delete;
    auto operator = (Parser&&) = delete;
    auto operator = (const Parser&) = delete;

    bool parse_object();
    bool parse_number();
    bool parse_string();
    bool parse_map();
    bool parse_list();

    template <typename T>
    bool expect(const char* seq, T value);

    void consume_whitespace();

    void create_error(const std::string& message);

    StreamIterator it;
    Object curr;
    std::string scratch{32};
    int error_offset = 0;
    std::string error_message;
};

template <typename StreamType>
bool Parser<StreamType>::parse_object()
{
    for (; !it.done(); it++) {
        switch (*it)
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                continue;

            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return parse_number();

            case '\'':
            case '"':
                return parse_string();

            case '[': return parse_list();
            case '{': return parse_map();

            case 't': return expect("true", true);
            case 'f': return expect("false", false);
            case 'n': return expect("null", (void*)0);

            default:
                break;
        }
    }

    if (error_message.size() == 0) {
        error_message = "No object in json stream";
    }

    return false;
}

template <typename StreamType>
bool Parser<StreamType>::parse_number() {
    scratch.clear();

    bool is_float = false;
    for (; !it.done(); it++) {
        char c = *it;
        switch (c) {
            case '.':
            case 'e':
            case 'E':
                is_float = true;
                break;
            case ',':
            case ' ':
            case ':':
            case '}':
            case ']':
            case '\0':
            case -1:
                goto DONE;
            default:
                break;
        }
        scratch.push_back(c);
    }

    DONE:
    const char* str = scratch.c_str();
    const char* scratch_end = str + scratch.size();
    char* end;
    if (is_float) {
        curr.free();
        curr = Object{strtod(str, &end)};
    } else {
        curr.free();
        curr = Object{strtoll(str, &end, 10)};
        if (errno) {
            errno = 0;
            curr.free();
            curr = Object{strtoull(str, &end, 10)};
        }
    }

    scratch.clear();

    if (errno) {
        create_error(strerror(errno));
        errno = 0;
        return false;
    } else if (end != scratch_end) {
        create_error("Numeric syntax error");
        return false;
    } else {
        return true;
    }
}

template <typename StreamType>
bool Parser<StreamType>::parse_string() {
    char quote = *it;
    it++;
    bool escape = false;
    std::string str;
    for(; !it.done(); it++) {
        char c = *it;
        if (c == '\\') {
            escape = true;
        } else if (!escape) {
            if (c == quote) { it++; break; }
            str.push_back(c);
        } else {
            escape = false;
            str.push_back(c);
        }
    }

    if (it.done()) {
        create_error("Unterminated string");
        return false;
    }

    curr.free();
    curr = std::move(str);
    return true;
}

template <typename StreamType>
bool Parser<StreamType>::parse_list() {
    List list;
    it++;  // consume [
    consume_whitespace();
    if (*it == ']') {
        curr.free();
        curr = std::move(list);
        return true;
    }
    while (!it.done()) {
        if (!parse_object()) return false;
        list.push_back(curr);
        consume_whitespace();
        char c = *it;
        if (c == ']') {
            it++;
            curr.free();
            curr = std::move(list);
            return true;
        } else if (c == ',') {
            it++;
            continue;
        }
    }

    create_error("Unterminated list");
    return false;
}

template <typename StreamType>
bool Parser<StreamType>::parse_map() {
    Map map;
    it++;  // consume {
    consume_whitespace();
    if (*it == '}') {
        curr.free();
        curr = std::move(map);
        return true;
    }

    std::pair<Key, Object> item;
    while (!it.done()) {
        // key
        if (!parse_object()) return false;

        std::get<0>(item) = curr.to_key();

        if (curr.is_container()) {
            create_error("Map keys must be a primitive type");
            return false;
        }

        consume_whitespace();
        char c = *it;
        if (c != ':') {
            create_error("Expected token ':'");
            return false;
        }

        // value
        if (!parse_object()) return false;
        std::get<1>(item) = curr;

        map.insert(item);
        consume_whitespace();

        c = *it;
        if (c == '}') {
            it++;
            curr.free();
            curr = std::move(map);
            return true;
        } else if (c == ',') {
            it++;
            continue;
        }
    }

    create_error("Unterminated map");
    return false;
}

template <typename StreamType>
template <typename T>
bool Parser<StreamType>::expect(const char* seq, T value) {
    const char* seq_it = seq;
    for (it++; !it.done(); it++, seq_it++) {
        if (*seq_it != *it) {
            create_error("Invalid literal");
            return false;
        }
    }
    curr.free();
    curr = value;
    return true;
}

template <typename StreamType>
void Parser<StreamType>::consume_whitespace()
{
    while (!it.done() && std::isspace(*it)) it++;
}

template <typename StreamType>
void Parser<StreamType>::create_error(const std::string& message)
{
    error_message = message;
    error_offset = it.consumed();
}

} // namespace impl


struct ParseError
{
    int error_offset = 0;
    std::string error_message;

    std::string to_str() const {
        if (error_message.size() > 0)
            return fmt::format("JSON parse error at {}: {}", error_offset, error_message);
        return "";
    }
};


inline
Object parse(const std::string& json, std::optional<ParseError>& error) {
    impl::Parser parser{std::istringstream{json}};
    if (!parser.parse_object()) {
        error = ParseError{parser.error_offset, std::move(parser.error_message)};
        return {};
    }
    return parser.curr;
}

inline
Object parse(const std::string& json, std::string& error) {
    std::optional<nodel::json::ParseError> parse_error;
    Object result = nodel::json::parse(json, parse_error);
    if (parse_error) {
        error = parse_error->to_str();
        return {};
    }
    return result;
}

inline
Object parse(const std::string& json) {
    impl::Parser parser{std::istringstream{json}};
    if (!parser.parse_object()) {
        return {};
    }
    return parser.curr;
}

}} // namespace nodel.json

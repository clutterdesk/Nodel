#pragma once

#include <ctype.h>
#include <cstdlib>
#include <cerrno>

#include "Object.h"


namespace nodel {

struct Parser
{
	template <typename T>
	Parser(T&& arg) : json{arg}, it{json.cbegin()}, end{json.cend()} {}

	bool parse_object();
	bool parse_number();
	bool parse_string();
	bool parse_map();
	bool parse_list();

	template <typename T>
	bool expect(const char* seq, T value);

	void consume_whitespace();

	void create_error(const std::string& message);

	std::string_view json;
	std::string_view::const_iterator it;
	std::string_view::const_iterator end;
	Object curr;
	std::string scratch{32};
	int error_offset = 0;
	std::string error_message;
};

inline
bool Parser::parse_object()
{
	for (; it != end; it++) {
		switch (*it)
		{
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			continue;

		case '+':  // extended support
			return parse_number();

		case '-':
			return parse_number();

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

inline
bool Parser::parse_number() {
	bool is_float = false;
	for (; it != end; it++) {
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
			goto DONE;
		default:
			break;
		}
		scratch.push_back(c);
	}

DONE:
	const char* str = scratch.c_str();
	char* end;
	if (is_float) {
		curr = Object{strtod(str, &end)};
	} else {
		curr = Object{strtoll(str, &end, 10)};
		if (errno) {
			errno = 0;
			curr = Object{strtoull(str, &end, 10)};
		}
	}

	scratch.clear();

	if (errno) {
		create_error(strerror(errno));
		errno = 0;
		return false;
	} else if (end == str) {
		create_error("Numeric syntax error");
		return false;
	} else {
		return true;
	}
}

inline
bool Parser::parse_string() {
	char quote = *it++;
	bool escape = false;
	std::string str;
	for(; it != end; it++) {
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

	if (it == end) {
		create_error("Unterminated string");
		return false;
	}

	curr = std::move(str);
	return true;
}

inline
bool Parser::parse_list() {
	Object obj{List()};
	List& list = obj.as_list();
	it++;  // consume [
	consume_whitespace();
	if (*it == ']') {
		curr = obj;
		return true;
	}
	while (it != end) {
		if (!parse_object()) return false;
		list.push_back(curr);
		consume_whitespace();
		char c = *it;
		if (c == ']') {
			it++;
			curr = obj;
			return true;
		} else if (c == ',') {
			it++;
			continue;
		}
	}

	create_error("Unterminated list");
	return false;
}

inline
bool Parser::parse_map() {
	Object obj{Map()};
	Map& map = obj.as_map();
	it++;  // consume {
	consume_whitespace();
	if (*it == '}') {
		curr = obj;
		return true;
	}

	std::pair<Key, Object> item;
	while (it != end) {
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
			curr = obj;
			return true;
		} else if (c == ',') {
			it++;
			continue;
		}
	}

	create_error("Unterminated map");
	return false;
}

template <typename T>
bool Parser::expect(const char* seq, T value) {
	const char* seq_it = seq;
	for (it++; it != end; it++, seq_it++) {
		if (*seq_it != *it) {
			create_error("Invalid literal");
			return false;
		}
	}
	curr = value;
	return true;
}

inline
void Parser::consume_whitespace()
{
	while (it != end && std::isspace(*it)) it++;
}

inline
void Parser::create_error(const std::string& message)
{
	error_message = message;
	error_offset = std::distance(it, end);
}

} // namespace nodel

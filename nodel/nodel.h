#include "impl/Object.h"
#include "impl/json.h"

namespace nodel {

inline
Object Object::from_json(const std::string& json, std::optional<nodel::json::ParseError>& error)
{
	Object result = nodel::json::parse(json, error);
	return error? Object{}: result;
}

inline
Object Object::from_json(const std::string& json, std::string& error) {
	error.clear();
	std::optional<nodel::json::ParseError> parse_error;
	Object result = nodel::json::parse(json, parse_error);
	if (parse_error) {
		error = parse_error->to_str();
		return {};
	}
	return result;
}


class JsonParseException : public std::exception
{
  public:
    JsonParseException(const std::string& msg) : msg(msg) {}
    const char* what() const throw() override { return msg.c_str(); }

  private:
    const std::string msg;
};

inline
Object Object::from_json(const std::string& json) {
	std::optional<nodel::json::ParseError> parse_error;
	Object result = nodel::json::parse(json, parse_error);
	if (parse_error) {
		throw JsonParseException(parse_error->to_str());
		return {};
	}
	return result;
}

} // namespace nodel

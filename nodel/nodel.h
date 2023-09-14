#include "impl/Object.h"
#include "impl/json.h"

namespace nodel {

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
	nodel::Parser parser{json};
	if (!parser.parse_object())
		throw JsonParseException(parser.error_message);
	return parser.curr;
}

} // namespace nodel

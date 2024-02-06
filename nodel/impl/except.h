#pragma once

namespace nodel {

class JsonException : public std::exception
{
  public:
    JsonException(const std::string& msg) : msg(msg) {}
    const char* what() const throw() override { return msg.c_str(); }

  private:
    const std::string msg;
};

} // namespace nodel

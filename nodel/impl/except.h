#pragma once

class InvalidException : public std::exception
{
  public:
    JsonParseException(const std::string& msg) : msg(msg) {}
    const char* what() const throw() override { return msg.c_str(); }

  private:
    const std::string msg;
};

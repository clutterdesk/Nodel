#pragma once

namespace nodel {

class ShortString
{
  public:
    ShortString();
    ShortString(char*);
    ShortString(char const*);

  private:
    std::variant<std::string, int32_t> dat;
};

class InternedStringBuilder
{
  public:
    void append(char);

  private:
    String str;
};


} // namespace nodel

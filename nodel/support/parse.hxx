/// @b License: @n Apache License v2.0
/// @copyright Robert Dunnagan
#pragma once

#include <algorithm>
#include <array>
#include <iomanip>
#include <nodel/support/exception.hxx>

namespace nodel::parse {

template <typename StreamType>
class StreamAdapter  // TODO: replace with std::stream_iterator?
{
  public:
    StreamAdapter(StreamType& stream) : m_stream{stream} {
        if (!m_stream.eof())
            fill();
    }

    char peek() { return m_buf[m_buf_pos]; }

    void next() {
        if (++m_buf_pos >= m_buf_size) {
            m_buf_pos = m_buf_size;
            if (!m_stream.eof())
                fill();
        }
    }

    size_t consumed() const { return m_pos + m_buf_pos; }
    bool done() const { return m_buf_pos == m_buf_size; }
    bool error() const { return m_stream.fail(); }

  private:
    void fill() {
        if (!m_stream.good()) {
            m_buf_size = 0;
            m_buf_pos = 0;
        } else {
            m_stream.read(m_buf.data(), m_buf.size());
            auto n_read = m_stream.gcount();
            if (n_read == 0) {
                m_buf_size = 0;
                m_buf_pos = 0;
            } else {
                m_pos += n_read;
                m_buf_size = n_read;
                m_buf_pos = 0;
                if (m_buf_size < m_buf.size())
                    m_buf[m_buf_size++] = 0;
            }
        }
    }

  private:
    StreamType& m_stream;
    size_t m_pos = 0;
    std::array<char, 4096> m_buf;
    size_t m_buf_pos = 0;
    size_t m_buf_size = 0;
};


template <typename StringType>
class StringStreamAdapter
{
  public:
    StringStreamAdapter(const StringType& str) : m_str{str} {}

    char peek() { return m_str[m_pos]; }
    void next() { ++m_pos; }
    size_t consumed() const { return m_pos; }
    bool done() const { return m_pos == m_str.size(); }
    bool error() const { return false; }

  private:
    StringType m_str;
    size_t m_pos = 0;
};


constexpr int syntax_context = 72;

struct SyntaxError : public NodelException
{
    static std::string make_message(const std::string_view& spec, std::ptrdiff_t offset, const std::string& message) {
        std::ptrdiff_t ctx_end = std::min(offset + syntax_context, (std::ptrdiff_t)spec.size());
        std::ptrdiff_t ctx_begin = std::max(ctx_end - syntax_context, (std::ptrdiff_t)0);
        std::stringstream ss;
        ss << message << " at offset " << offset << std::endl;
        auto it = spec.cbegin();
        auto end = it + ctx_end;
        it += ctx_begin;
        for (; it != end; ++it) ss << *it;
        ss << std::endl;
        ss << std::setfill('-') << std::setw(offset - ctx_begin + 1) << '^';
        return ss.str();
    }

    SyntaxError(const std::string_view& spec, std::ptrdiff_t offset, const std::string& message)
      : NodelException(make_message(spec, offset, message)) {}
};

} // namespace nodel::parse

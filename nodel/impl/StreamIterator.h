#pragma once

namespace nodel {
namespace impl {

template <typename StreamType>
struct StreamIterator  // TODO: replace with std::stream_iterator?
{
    StreamIterator(StreamType& stream) : m_stream{stream} {
        if (!m_stream.eof())
            fill();
    }

    StreamIterator(StreamIterator&&) = default;
    StreamIterator(const StreamIterator&) = delete;
    auto operator = (StreamIterator&&) = delete;
    auto operator = (StreamIterator&) = delete;

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

    void fill() {
        m_pos += m_buf_size;
        m_stream.read(m_buf.data(), m_buf.size());
        m_buf_size = m_stream.gcount();
        m_buf_pos = 0;
        if (m_buf_size < m_buf.size())
            m_buf[m_buf_size++] = 0;
    }

    StreamType& m_stream;
    size_t m_pos = 0;
    std::array<char, 4096> m_buf;
    size_t m_buf_pos = 0;
    size_t m_buf_size = 0;
};

} // namespace impl
} // namespace nodel

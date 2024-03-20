// Copyright 2024 Robert A. Dunnagan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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

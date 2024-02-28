#pragma once

#include <chrono>
#include <algorithm>
#include <numeric>
#include <vector>

using namespace std::chrono;

namespace nodel {
namespace debug {

class Stopwatch
{
  public:
    Stopwatch(bool automatic=false)                          : m_name{"stopwatch"}, m_automatic(automatic) { if (automatic) start(); }
    Stopwatch(const std::string& name, bool automatic=false) : m_name{name}, m_automatic(automatic) { if (automatic) start(); }
    ~Stopwatch() { stop(); if (m_automatic) log(); }

    void start() { m_running = true; m_t_start = high_resolution_clock::now(); }
    void stop()  {
        if (m_running) {
            m_history.push_back(duration_cast<nanoseconds>(high_resolution_clock::now() - m_t_start).count());
            m_running = false;
        }
    }

    auto last()  { return m_history.back() / 1e6; }
    auto min()   { return *std::min_element(m_history.begin(), m_history.end()) / 1e6; }
    auto max()   { return *std::max_element(m_history.begin(), m_history.end()) / 1e6; }
    auto total() { return std::accumulate(m_history.begin(), m_history.end(), 0) / 1e6; }
    auto avg()   { return (double)total() / m_history.size() / 1e6; }

    void clear()   { m_history.clear(); }
    void log();

  private:
    std::string m_name;
    bool m_automatic;
    time_point<high_resolution_clock> m_t_start;
    std::vector<uint64_t> m_history;
    bool m_running = false;
};

inline
void Stopwatch::log() {
    if (m_history.size() == 1) {
        std::cout << m_name << ": " << last() << " ms" << std::endl;
    } else {
        std::cout << m_name << ": total=" << total() << " ms, avg=" << avg()
                  << " ms, min=" << min() << " ms, max=" << max() << std::endl;
    }
}

} // namespace debug
} // namespace nodel

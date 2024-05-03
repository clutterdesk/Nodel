#pragma once

#include <chrono>
#include <algorithm>
#include <numeric>
#include <vector>
#include <iostream>

using namespace std::chrono;

namespace nodel {
namespace debug {

class Stopwatch
{
  public:
    Stopwatch(bool automatic=false)                          : m_name{"stopwatch"}, m_automatic(automatic) { if (automatic) start(); }
    Stopwatch(const std::string& name, bool automatic=false) : m_name{name}, m_automatic(automatic) { if (automatic) start(); }
    ~Stopwatch() { stop(); if (m_automatic) log(); }

    template <typename Func>
    void measure(float seconds, Func&& func) {
        do {
            start();
            func();
            stop();
            seconds -= m_history.back() / 1e9;
        } while (seconds > 0);
    }

    void start() {
        m_running = true;
        m_t_start = high_resolution_clock::now();
    }

    void start(const std::string& name) {
        m_name = name;
        start();
    }

    void stop()  {
        if (m_running) {
            m_history.push_back(duration_cast<nanoseconds>(high_resolution_clock::now() - m_t_start).count());
            m_running = false;
        }
    }

    auto finish() {
        stop();
        log();
        auto elapsed = m_history.back();
        clear();
        return elapsed;
    }

    auto last()  { return m_history.back(); }
    auto min()   { return *std::min_element(m_history.begin(), m_history.end()); }
    auto max()   { return *std::max_element(m_history.begin(), m_history.end()); }
    auto total() { return std::accumulate(m_history.begin(), m_history.end(), 0ULL); }
    auto avg()   { return (double)total() / m_history.size(); }

    void clear()   { m_history.clear(); }
    void format(int64_t t_ns, std::ostream& os);
    void log();

  private:
    std::string m_name;
    bool m_automatic;
    time_point<high_resolution_clock> m_t_start;
    std::vector<uint64_t> m_history;
    bool m_running = false;
};

inline
void Stopwatch::format(int64_t t_ns, std::ostream& os) {
    if (t_ns < 1e3) { os << t_ns << " ns"; }
    else if (t_ns < 1e6) { os << (t_ns / 1e3) << " us"; }
    else if (t_ns < 1e9) { os << (t_ns / 1e6) << " ms"; }
    else { os << (t_ns / 1e9) << " s"; }
}

inline
void Stopwatch::log() {
    if (m_history.size() == 0) {
        std::cout << m_name << ": no data" << std::endl;
    } else if (m_history.size() == 1) {
        std::cout << m_name << ": ";
        format(last(), std::cout);
        std::cout << std::endl;
    } else {
        std::cout << m_name << ": runs=" << (float)m_history.size() << ", total=";
        format(total(), std::cout);
        std::cout << ", avg=";
        format(avg(), std::cout);
        std::cout << ", min=";
        format(min(), std::cout);
        std::cout << ", max=";
        format(max(), std::cout);
        std::cout << std::endl;
    }
}

} // namespace debug
} // namespace nodel

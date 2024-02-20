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
    Stopwatch(bool automatic=false)                          : name{"stopwatch"}, automatic(automatic) { if (automatic) start(); }
    Stopwatch(const std::string& name, bool automatic=false) : name{name}, automatic(automatic) { if (automatic) start(); }
    ~Stopwatch() { stop(); if (automatic) log(); }

    void start() { running = true; t_start = high_resolution_clock::now(); }
    void stop()  {
        if (running) {
            history.push_back(duration_cast<nanoseconds>(high_resolution_clock::now() - t_start).count());
            running = false;
        }
    }

    auto last()  { return history.back() / 1e6; }
    auto min()   { return *std::min_element(history.begin(), history.end()) / 1e6; }
    auto max()   { return *std::max_element(history.begin(), history.end()) / 1e6; }
    auto total() { return std::accumulate(history.begin(), history.end(), 0) / 1e6; }
    auto avg()   { return (double)total() / history.size() / 1e6; }

    void clear()   { history.clear(); }
    void log();

  private:
    std::string name;
    bool automatic;
    time_point<high_resolution_clock> t_start;
    std::vector<uint64_t> history;
    bool running = false;
};

inline
void Stopwatch::log() {
    if (history.size() == 1) {
        std::cout << name << ": " << last() << " ms" << std::endl;
    } else {
        std::cout << name << ": total=" << total() << " ms, avg=" << avg()
                  << " ms, min=" << min() << " ms, max=" << max() << std::endl;
    }
}

} // namespace debug
} // namespace nodel

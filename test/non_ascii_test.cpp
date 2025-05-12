/*
Tests performance on non-ASCII data (namely random bytes)
*/

#include "../src/gql.hpp"
#include <climits>
#include <iostream>
#include <random>

// Generates _n uniform* bytes (*excluding the null terminator)
// and outputs it as a std::string
std::string random_bytes(const uint &_n) {
  static std::random_device rng;
  static std::uniform_int_distribution<char> dist(CHAR_MIN,
                                                  CHAR_MAX);
  std::string out;
  for (uint i = 0; i < _n; ++i) {
    char next;
    do {
      next = dist(rng);
    } while (next == '\0');
    out.push_back(next);
  }
  return out;
}

int main() {
  GQL g;
  std::cout << "Encoding test started.\n" << std::flush;

  for (uint i = 0; i < 512; ++i) {
    const auto label = random_bytes(32 + i);
    const auto key = random_bytes(8 + i);
    const auto value = random_bytes(256 + i);

    auto n = g.add_vertex().label(label).tag(key, value);

    assert(n.label()["label"].front() == label);
    assert(n.keys() == std::list<std::string>{key});
    assert(n.tag(key)[key].front() == value);
  }

  std::cout << "Encoding test passed.\n";

  return 0;
}

/**
 * @brief Measures Shannon entropy of letters in a text data
 * file. Sidenote: This would be an excellent example in a data
 * structures class if we used a 27-trie instead of GQL

 (probably wrong) equation:

 If "d" in "abcd" has n bits of entropy, then:
 Count(abc) = 2^n * Count(abcd)
 -> log_2(Count(abc) / Count(abcd)) = n
 */

#include "../src/gql.hpp"

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stack>
#include <string>

/// Instance that counts occurrances
GQL gdb(__FILE__ ".db", true);

/// Cleans an input string, possibly reducing it to empty
std::string clean(const std::string &_what);

/// Adds a single instance of the given word, cleaning it first
void add(const std::string &_what);

int main() {
  const std::string filepath = "bohemia.txt";

  // Begin file processing timer
  std::cout << "Processing " << filepath << "...\n";
  auto start = std::chrono::high_resolution_clock::now();

  // Open as file
  std::ifstream file(filepath, std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "Failed to open!\n";
    return 0;
  }

  // Get size
  const auto size = file.tellg();
  file.seekg(std::ios::beg);

  // For each word
  uint j = 0;
  for (std::string word; !file.eof(); file >> word) {
    // Feed into counter
    add(word);

    if (j % 100 == 0) {
      // Print position in file
      std::cout << (100.0 *
                    ((double)file.tellg() / (double)size))
                << "%:\t" << clean(word) << "\n";
    }
    ++j;
  }

  // Output timer
  auto stop = std::chrono::high_resolution_clock::now();
  auto elapsed_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          stop - start)
          .count();
  std::cout << "Took " << elapsed_ns / 1'000'000.0 << " ms\n"
            << std::flush;

  // Compute entropy (depth-first)
  double sum = 0.0;
  uintmax_t n = 0;

  std::stack<GQL::Vertices> to_visit;
  to_visit.push(gdb.v().with_label("START"));

  // A -(b)-> B -(c)->
  while (!to_visit.empty()) {
    // (A)
    auto node_a = to_visit.top();
    to_visit.pop();

    // For each edge out of here (b)
    for (const auto &edge_b : node_a.out().each()) {
      // Get count of (b)
      const uintmax_t b =
          std::stoi(edge_b.tag("COUNT")["COUNT"][0]);

      // Get the target (B)
      auto node_b = edge_b.target();

      // Push (B) to stack
      to_visit.push(node_b);

      // For each edge out of here (c)
      for (const auto &edge_c : node_b.out().each()) {
        // Get count of (c)
        const uintmax_t c =
            std::stoi(edge_c.tag("COUNT")["COUNT"][0]);
        const double shannons = log2((double)b / (double)c);
        sum += shannons;
        ++n;
      }
    }
  }

  std::cout << "Mean Shannons per letter over " << n
            << " instances: " << (sum / (double)n) << '\n';

  return 0;
}

////////////////////////////////////////////////////////////////

/// Cleans an input string, possibly reducing it to empty
std::string clean(const std::string &_what) {
  std::string out;
  for (const char &c : _what) {
    if (std::isalpha(c)) {
      out.push_back(std::tolower(c));
    }
  }
  return out;
}

/// Adds a single instance of the given word, cleaning it first
void add(const std::string &_what) {
  const std::string cleaned = clean(_what);
  if (cleaned.empty()) {
    return;
  }

  // Start on entry node
  auto cur = gdb.v().with_label("START");
  if (cur.empty()) {
    cur = gdb.add_vertex().label("START");
  }

  // For each letter
  std::string partial;
  for (const char &c : cleaned) {
    partial.push_back(c);

    if (cur.out().with_label({c}).exists()) {
      // Increment
      auto current = std::stoi(
          cur.out().with_label({c}).tag("COUNT")["COUNT"][0]);
      cur.out().with_label({c}).tag(
          "COUNT", std::to_string(current + 1));
    } else {
      // Set to 1
      cur.add_edge(gdb.add_vertex())
          .label({c})
          .tag("COUNT", "1");
    }
    cur = cur.out().with_label({c}).target();
  }
}

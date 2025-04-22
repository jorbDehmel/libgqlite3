/*
A GQL CLI/parser for fun.
This software is licensed as part of GQL.
Jordan Dehmel, '24 - '25
*/

#include "../src/gql.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

static_assert(__cplusplus >= 2020'00UL);
static_assert(GQL_VERSION >= 000'002'002UL);

#define assert_holds_alt(val, alt)                             \
  {                                                            \
    if (std::holds_alternative<alt>(val)) {                    \
    } else {                                                   \
      std::cerr << __FILE__ << ":" << __LINE__ << "\n"         \
                << std::flush;                                 \
      if (std::holds_alternative<std::string>(val)) {          \
        throw std::runtime_error(                              \
            #val " held std::string instead of " #alt);        \
      } else if (std::holds_alternative<GQL::Result>(val)) {   \
        throw std::runtime_error(                              \
            #val " held GQL::Result instead of " #alt);        \
      } else if (std::holds_alternative<GQL::Vertices>(val)) { \
        throw std::runtime_error(                              \
            #val " held GQL::Vertices instead of " #alt);      \
      } else if (std::holds_alternative<GQL::Edges>(val)) {    \
        throw std::runtime_error(                              \
            #val " held GQL::Edges instead of " #alt);         \
      } else if (std::holds_alternative<std::shared_ptr<GQL>>( \
                     val)) {                                   \
        throw std::runtime_error(                              \
            #val                                               \
            " held std::shared_ptr<GQL> instead of " #alt);    \
      }                                                        \
    }                                                          \
  }

// An allowed variable type
using VarType =
    std::variant<std::string, GQL::Result, GQL::Vertices,
                 GQL::Edges, std::shared_ptr<GQL>>;

// Optional version of VarType
using OptVarType = std::optional<VarType>;

// A function mapping a operand (or none) and zero or more
// arguments to zero or one return value.
using OpType =
    std::function<OptVarType(OptVarType, std::vector<VarType>)>;

// Contains settings to be parsed
struct Settings {
  // If present, script. Else, stdin
  std::optional<std::string> input_path;
};

// Lex TEXT
std::vector<std::string> lex(const std::string &_text) {
  std::vector<std::string> out;
  std::string cur;

  char enclosure = '\0';
  bool ignore = false;
  for (const auto &c : _text) {
    if (ignore) {
      ignore = false;
      continue;
    } else if (enclosure != '\0') {
      cur.push_back(c);
      if (c == '\\') {
        ignore = true;
      } else if (c == enclosure) {
        if (enclosure != '\n') {
          out.push_back(cur);
        }
        enclosure = '\0';
        cur.clear();
      }
    } else if (c == '\'' || c == '"') {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
      cur.push_back(c);
      enclosure = c;
    } else if (c == ' ' || c == '\t' || c == '\n') {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else if (c == '.' || c == '(' || c == ')') {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
      out.push_back({c});
    } else {
      cur.push_back(c);

      if (cur == "//") {
        enclosure = '\n';
      }
    }
  }
  if (!cur.empty()) {
    out.push_back(cur);
  }

  if (enclosure != '\0') {
    throw std::runtime_error("Unterminated enclosure `" +
                             std::string{enclosure} + "`");
  }

  return out;
}

// Lex a FILE
std::vector<std::string>
lex_file(const std::filesystem::path &_fp) {
  std::string text;
  std::ifstream f(_fp);
  assert(f.is_open());
  text.assign(std::istreambuf_iterator<char>(f),
              std::istreambuf_iterator<char>());
  return lex(text);
}

void print_help() {
  std::cout << "GQL CLI\n\n"
            << " Flag    | Meaning\n"
            << "---------|-----------------------\n"
            << " --help  | Show help text (this)\n"
            << " --input | Execute from a script\n\n"
            << "Part of GQL, licensed under MIT licence. "
               "Jordan Dehmel, 2024-2025.\n"
            << "GQL " << GQL_MAJOR_VERSION << "."
            << GQL_MINOR_VERSION << "." << GQL_PATCH_VERSION
            << '\n';
}

void print_variable(const VarType &_what) {
  if (std::holds_alternative<std::string>(_what)) {
    // String literal
    std::cout << '"' << std::get<std::string>(_what) << '"';
  } else if (std::holds_alternative<GQL::Result>(_what)) {
    // Result object
    std::cout << std::get<GQL::Result>(_what);
  } else if (std::holds_alternative<GQL::Vertices>(_what)) {
    // Vertex object
    for (const auto &v :
         std::get<GQL::Vertices>(_what).each()) {
      std::cout << "+ " << v.id().front() << " '"
                << v.label()["label"][0] << "'";
      for (const std::string &key : v.keys()) {
        std::cout << '\n'
                  << "|- '" << key
                  << "': " << v.tag(key)[key][0];
      }
      std::cout << '\n';
    }
  } else if (std::holds_alternative<std::shared_ptr<GQL>>(
                 _what)) {
    // GQL object
    std::cout
        << "+ Graph object at '"
        << std::get<std::shared_ptr<GQL>>(_what)->get_filepath()
        << "' w/ "
        << std::get<std::shared_ptr<GQL>>(_what)
               ->sql_call_counter
        << " SQL calls\n";
  } else {
    // Edge object
    for (const auto &v : std::get<GQL::Edges>(_what).each()) {
      std::cout << "+ " << v.id().front() << ": "
                << v.source().id().front() << " -> "
                << v.target().id().front() << " '"
                << v.label()["label"][0] << "'";
      for (const std::string &key : v.keys()) {
        std::cout << '\n'
                  << "|- '" << key
                  << "': " << v.tag(key)[key][0];
      }
      std::cout << '\n';
    }
  }
  std::cout << '\n';
}

void dump_variables(
    const std::map<std::string, VarType> &_variables) {
  // Dump variables
  std::cout << "All variables:\n";
  for (const auto &p : _variables) {
    std::cout << "Variable `" << p.first << "`:\n";
    print_variable(p.second);
  }
}

int main(int c, char *v[]) {
  Settings settings;

  // Load from command-line arguments
  for (int i = 0; i < c; ++i) {
    if (strcmp(v[i], "--help") == 0) {
      print_help();
      return 0;
    } else if (strcmp(v[i], "--input") == 0) {
      if (i + 1 >= c) {
        return 1;
      }
      settings.input_path = v[i + 1];
      ++i;
    }
  }

  bool is_running = true;
  std::map<std::string, VarType> variables;

  // Maps function names to their lambda definitions
  const std::map<std::string, OpType> operations = {
      {
          "GQL",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            // Open a new GQL instance
            assert(!_what.has_value() && _args.size() <= 3);
            std::string db_path = ":memory:";
            bool erase = false, persistence = true;

            // No args: In-memory
            if (_args.size() == 0) {
              erase = persistence = false;
            }

            // 1 arg: Save to disk
            if (_args.size() >= 1) {
              db_path = std::get<std::string>(_args.at(0));
            }

            // 2 args: Save to disk, erasure setting
            if (_args.size() >= 2) {
              erase = (std::get<std::string>(_args.at(1)) ==
                       "true");
            }

            // 3 args: Save to disk, erasure, persistence
            if (_args.size() == 3) {
              persistence = (std::get<std::string>(
                                 _args.at(2)) == "true");
            }
            return std::make_shared<GQL>(db_path, erase,
                                         persistence);
          },
      },
      {
          "filepath",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.empty());
            assert_holds_alt(_what.value(),
                             std::shared_ptr<GQL>);
            return std::get<std::shared_ptr<GQL>>(_what.value())
                ->get_filepath()
                .string();
          },
      },
      {
          "q",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(!_what.has_value() && _args.empty());
            is_running = false;
            return {};
          },
      },
      {
          "v",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            // Get all vertices
            assert(_what.has_value());
            assert_holds_alt(_what.value(),
                             std::shared_ptr<GQL>);
            assert(_args.empty());
            return std::get<std::shared_ptr<GQL>>(_what.value())
                ->v();
          },
      },
      {
          "e",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            // Get all edges
            assert(_what.has_value());
            assert_holds_alt(_what.value(),
                             std::shared_ptr<GQL>);
            assert(_args.empty());
            return std::get<std::shared_ptr<GQL>>(_what.value())
                ->e();
          },
      },
      {
          "graphviz",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value());
            assert_holds_alt(_what.value(),
                             std::shared_ptr<GQL>);
            assert(_args.size() == 1);
            std::get<std::shared_ptr<GQL>>(_what.value())
                ->graphviz(std::get<std::string>(_args.at(0)));
            return {};
          },
      },
      {
          "commit",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value());
            assert_holds_alt(_what.value(),
                             std::shared_ptr<GQL>);
            assert(_args.empty());
            std::get<std::shared_ptr<GQL>>(_what.value())
                ->commit();
            return {};
          },
      },
      {
          "rollback",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value());
            assert_holds_alt(_what.value(),
                             std::shared_ptr<GQL>);
            assert(_args.empty());
            std::get<std::shared_ptr<GQL>>(_what.value())
                ->rollback();
            return {};
          },
      },
      {
          "add_vertex",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value());
            assert_holds_alt(_what.value(),
                             std::shared_ptr<GQL>);
            assert(_args.empty());
            return std::get<std::shared_ptr<GQL>>(_what.value())
                ->add_vertex();
          },
      },
      {
          "as",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.size() == 1 && _what.has_value());
            assert_holds_alt(_args.at(0), std::string);
            assert(!operations.contains(
                std::get<std::string>(_args.at(0))));

            variables[std::get<std::string>(_args.at(0))] =
                _what.value();
            return {};
          },
      },
      {
          "with_label",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.size() == 1 && _what.has_value());
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              return std::get<GQL::Vertices>(_what.value())
                  .with_label(
                      std::get<std::string>(_args.at(0)));
            } else {
              return std::get<GQL::Edges>(_what.value())
                  .with_label(
                      std::get<std::string>(_args.at(0)));
            }
          },
      },
      {
          "with_tag",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.size() == 2);
            assert(_what.has_value());
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              return std::get<GQL::Vertices>(_what.value())
                  .with_tag(std::get<std::string>(_args.at(0)),
                            std::get<std::string>(_args.at(1)));
            } else {
              return std::get<GQL::Edges>(_what.value())
                  .with_tag(std::get<std::string>(_args.at(0)),
                            std::get<std::string>(_args.at(1)));
            }
          },
      },
      {
          "with_id",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.size() == 1 && _what.has_value());
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              return std::get<GQL::Vertices>(_what.value())
                  .with_id(std::stoi(
                      std::get<std::string>(_args.at(0))));
            } else {
              return std::get<GQL::Edges>(_what.value())
                  .with_id(std::stoi(
                      std::get<std::string>(_args.at(0))));
            }
          },
      },
      {
          "join",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.size() == 1 && _what.has_value());
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              return std::get<GQL::Vertices>(_what.value())
                  .join(std::get<GQL::Vertices>(_args.at(0)));
            } else {
              return std::get<GQL::Edges>(_what.value())
                  .join(std::get<GQL::Edges>(_args.at(0)));
            }
          },
      },
      {
          "intersection",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.size() == 1 && _what.has_value());
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              return std::get<GQL::Vertices>(_what.value())
                  .intersection(
                      std::get<GQL::Vertices>(_args.at(0)));
            } else {
              assert_holds_alt(_args.at(0), GQL::Edges);
              return std::get<GQL::Edges>(_what.value())
                  .intersection(
                      std::get<GQL::Edges>(_args.at(0)));
            }
          },
      },
      {
          "complement",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.size() == 1 && _what.has_value());
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              return std::get<GQL::Vertices>(_what.value())
                  .complement(
                      std::get<GQL::Vertices>(_args.at(0)));
            } else {
              return std::get<GQL::Edges>(_what.value())
                  .complement(
                      std::get<GQL::Edges>(_args.at(0)));
            }
          },
      },
      {
          "label",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.size() == 0 || _args.size() == 1);
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              if (_args.empty()) {
                return std::get<GQL::Vertices>(_what.value())
                    .label();
              } else {
                return std::get<GQL::Vertices>(_what.value())
                    .label(std::get<std::string>(_args.at(0)));
              }
            } else {
              if (_args.empty()) {
                return std::get<GQL::Edges>(_what.value())
                    .label();
              } else {
                return std::get<GQL::Edges>(_what.value())
                    .label(std::get<std::string>(_args.at(0)));
              }
            }
          },
      },
      {
          "tag",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.size() > 0 &&
                   _args.size() < 3);
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              if (_args.size() == 1) {
                return std::get<GQL::Vertices>(_what.value())
                    .tag(std::get<std::string>(_args.at(0)));
              } else {
                return std::get<GQL::Vertices>(_what.value())
                    .tag(std::get<std::string>(_args.at(0)),
                         std::get<std::string>(_args.at(1)));
              }
            } else {
              if (_args.size() == 1) {
                return std::get<GQL::Edges>(_what.value())
                    .tag(std::get<std::string>(_args.at(0)));
              } else {
                return std::get<GQL::Edges>(_what.value())
                    .tag(std::get<std::string>(_args.at(0)),
                         std::get<std::string>(_args.at(1)));
              }
            }
          },
      },
      {
          "id",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.empty() && _what.has_value());
            std::list<uint64_t> to_convert;
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              to_convert =
                  std::get<GQL::Vertices>(_what.value()).id();
            } else {
              to_convert =
                  std::get<GQL::Edges>(_what.value()).id();
            }
            GQL::Result out;
            out.headers = {"id"};
            for (const auto &item : to_convert) {
              out.body.push_back({std::to_string(item)});
            }
            return out;
          },
      },
      {
          "erase",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_args.empty() && _what.has_value());
            if (std::holds_alternative<GQL::Vertices>(
                    _what.value())) {
              std::get<GQL::Vertices>(_what.value()).erase();
              return {};
            } else {
              std::get<GQL::Edges>(_what.value()).erase();
              return {};
            }
          },
      },
      {
          "add_edge",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.size() == 1);
            return std::get<GQL::Vertices>(_what.value())
                .add_edge(std::get<GQL::Vertices>(_args.at(0)));
          },
      },
      {
          "in",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.empty());
            return std::get<GQL::Vertices>(_what.value()).in();
          },
      },
      {
          "out",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.empty());
            return std::get<GQL::Vertices>(_what.value()).out();
          },
      },
      {
          "with_in_degree",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.size() > 0 &&
                   _args.size() < 3);
            assert(_args.size() == 1);
            return std::get<GQL::Vertices>(_what.value())
                .with_in_degree(std::stoi(
                    std::get<std::string>(_args.at(0))));
          },
      },
      {
          "with_out_degree",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.size() > 0 &&
                   _args.size() < 3);
            assert(_args.size() == 1);
            return std::get<GQL::Vertices>(_what.value())
                .with_out_degree(std::stoi(
                    std::get<std::string>(_args.at(0))));
          },
      },
      {
          "with_source",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.size() == 1);
            return std::get<GQL::Edges>(_what.value())
                .with_source(
                    std::get<GQL::Vertices>(_args.at(0)));
          },
      },
      {
          "with_target",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.size() == 1);
            return std::get<GQL::Edges>(_what.value())
                .with_target(
                    std::get<GQL::Vertices>(_args.at(0)));
          },
      },
      {
          "source",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.empty());
            return std::get<GQL::Edges>(_what.value()).source();
          },
      },
      {
          "target",
          [&](OptVarType _what,
              std::vector<VarType> _args) -> OptVarType {
            assert(_what.has_value() && _args.empty());
            return std::get<GQL::Edges>(_what.value()).target();
          },
      },
  };

  // Process instructions (recursive descent)
  const std::function<OptVarType(
      const std::vector<std::string> &, uint &)>
      do_stmt = [&](const std::vector<std::string> &inp,
                    uint &pos) -> OptVarType {
    // The running return value
    OptVarType out;

    // Main loop
    do {
      if (inp[pos] == ".") {
        ++pos;
      }

      // Name
      const auto name = inp[pos];

      // Function call
      if (pos < inp.size() && inp[pos + 1] == "(") {
        // Collect arguments
        ++pos;
        std::vector<VarType> args;
        if (inp[pos + 1] != ")") {
          do {
            ++pos;
            const auto r = do_stmt(inp, pos);
            if (r.has_value()) {
              args.push_back(r.value());
            }
          } while (inp[pos] == ",");
        } else {
          ++pos;
        }
        assert(inp[pos] == ")");

        if (!operations.contains(name)) {
          throw std::runtime_error("Invalid method '" + name +
                                   "'");
        }
        const auto op = operations.at(name);
        out = op(out, args);
      }

      // String literal
      else if (name.front() == '\'' || name.front() == '"') {
        out = inp[pos].substr(1, inp[pos].size() - 2);
      }

      // Var name
      else if (variables.contains(name)) {
        out = variables.at(name);
      }

      // Error case
      else {
        throw std::runtime_error("Symbol `" + name +
                                 "` could not be resolved.");
      }
      ++pos;
    } while (pos < inp.size() && inp[pos] == ".");
    return out;
  };

  // Call the parser
  try {
    if (settings.input_path.has_value()) {
      // From file
      uint pos = 0;
      const std::vector<std::string> inp =
          lex_file(settings.input_path.value());
      while (pos < inp.size() && is_running) {
        const auto result = do_stmt(inp, pos);
        assert(inp[pos] == ";");
        ++pos;
        if (result.has_value()) {
          print_variable(result.value());
        }
      }
    } else {
      // From stdin
      uint pos = 0, line = 1;
      std::vector<std::string> accumulator;
      while (is_running && !std::cin.eof()) {
        while (accumulator.empty() ||
               accumulator.back() != ";") {
          std::cout << line++ << "> ";
          std::string line;
          std::getline(std::cin, line);
          line += "\n";
          if (std::cin.eof()) {
            break;
          }

          for (const auto &tok : lex(line)) {
            accumulator.push_back(tok);
          }
        }

        pos = 0;
        while (pos < accumulator.size() && is_running) {
          const auto result = do_stmt(accumulator, pos);
          assert(accumulator[pos] == ";");
          ++pos;
          if (result.has_value()) {
            print_variable(result.value());
          }
        }
        accumulator.clear();
      }
    }
  } catch (std::runtime_error &_e) {
    for (int i = 0; i < 64; ++i) {
      std::cerr << '~';
    }
    std::cerr << "\nError: " << _e.what() << "\n";
    dump_variables(variables);
    return 2;
  } catch (...) {
    std::cerr << "Unknown error occurred\n";
    dump_variables(variables);
    return 3;
  }

  dump_variables(variables);
  return 0;
}

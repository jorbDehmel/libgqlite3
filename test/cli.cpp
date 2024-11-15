/*
A CLI/parser for GQL for fun.
This is only slightly smaller than GQL itself.
This software is licensed as part of GQL.
Jordan Dehmel, '24
*/

#include "../src/gql.hpp"

#include <cassert>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

static_assert(__cplusplus >= 2020'00UL);
static_assert(GQL_VERSION >= 000'000'003UL);

typedef std::variant<std::string, GQL::Result, GQL::Vertices,
                     GQL::Edges>
    VarType;

// Optional version of VarType
typedef std::optional<std::variant<std::string, GQL::Result,
                                   GQL::Vertices, GQL::Edges>>
    OptVarType;

// A function mapping a operand (or none) and zero or more
// arguments to zero or one return value.
typedef std::function<OptVarType(OptVarType,
                                 std::vector<VarType>)>
    OpType;

std::vector<std::string> lex(const std::string &_fp)
{
    std::string text, cur;
    std::vector<std::string> out;
    std::ifstream f(_fp);
    text.assign(std::istreambuf_iterator<char>(f),
                std::istreambuf_iterator<char>());

    char enclosure = '\0';
    bool ignore = false;
    for (const auto &c : text)
    {
        if (ignore)
        {
            ignore = false;
            continue;
        }
        else if (enclosure != '\0')
        {
            cur.push_back(c);
            if (c == '\\')
            {
                ignore = true;
            }
            else if (c == enclosure)
            {
                if (enclosure != '\n')
                {
                    out.push_back(cur);
                }
                enclosure = '\0';
                cur.clear();
            }
        }
        else if (c == '\'' || c == '"')
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
            cur.push_back(c);
            enclosure = c;
        }
        else if (c == ' ' || c == '\t' || c == '\n')
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else if (c == '.' || c == '(' || c == ')')
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
            out.push_back({c});
        }
        else
        {
            cur.push_back(c);

            if (cur == "//")
            {
                enclosure = '\n';
            }
        }
    }
    if (!cur.empty())
    {
        out.push_back(cur);
    }

    if (enclosure != '\0')
    {
        throw std::runtime_error("Unterminated enclosure `" +
                                 std::string{enclosure} + "`");
    }

    return out;
}

int main(int c, char *v[])
{
    std::string db_path = "gql_tmp.db", inp_path;
    bool erase = false;

    // Load from command-line arguments
    switch (c)
    {
    case 1:
        std::cerr << "Please provide a source file and "
                     "optionally a database file.\n"
                     "If 'erase' is the third argument, the DB "
                     "will be wiped upon instantiation.\n"
                     "Part of GQL, licensed under MIT licence. "
                     "Jordan Dehmel, 2024.\n";
        return 1;
    default:
    case 4:
        if (strcmp(v[3], "erase") == 0)
        {
            erase = true;
        }
    case 3:
        db_path = v[2];
    case 2:
        inp_path = v[1];
    }

    uint pos = 0;
    bool is_running = true;
    const std::vector<std::string> inp = lex(inp_path);
    GQL gql(db_path, erase);
    std::map<std::string, VarType> variables;

    const std::map<std::string, OpType> operations = {
        {"q",
         [&is_running](OptVarType,
                       std::vector<VarType>) -> OptVarType {
             is_running = false;
             return {};
         }},
        {"v",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             // Get all vertices
             assert(!_what.has_value() && _args.empty());
             return gql.v();
         }},
        {"e",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             // Get all edges
             assert(!_what.has_value() && _args.empty());
             return gql.e();
         }},
        {"graphviz",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(!_what.has_value());
             assert(
                 _args.size() == 1 &&
                 std::holds_alternative<std::string>(_args[0]));
             gql.graphviz(std::get<std::string>(_args[0]));
             return {};
         }},
        {"commit",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(!_what.has_value());
             assert(_args.empty());
             gql.commit();
             return {};
         }},
        {"rollback",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(!_what.has_value());
             assert(_args.empty());
             gql.rollback();
             return {};
         }},
        {"add_vertex",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(!_what.has_value());
             assert(_args.empty());
             return gql.add_vertex();
         }},
        {"as",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             // Alias _what as _args[0]
             assert(
                 _args.size() == 1 && _what.has_value() &&
                 std::holds_alternative<std::string>(_args[0]));
             variables[std::get<std::string>(_args[0])] =
                 _what.value();
             return {};
         }},
        {"where",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             // SQL where clause
             assert(_what.has_value());
             assert(std::holds_alternative<GQL::Edges>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Vertices>(
                        _what.value()));
             assert(
                 _args.size() == 1 &&
                 std::holds_alternative<std::string>(_args[0]));

             if (std::holds_alternative<GQL::Edges>(
                     _what.value()))
             {
                 return std::get<GQL::Edges>(_what.value())
                     .where(std::get<std::string>(_args[0]));
             }
             else
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .where(std::get<std::string>(_args[0]));
             }
         }},
        {"with_label",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.size() == 1 &&
                    std::holds_alternative<std::string>(
                        _args[0]) &&
                    _what.has_value());
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .with_label(
                         std::get<std::string>(_args[0]));
             }
             else
             {
                 return std::get<GQL::Edges>(_what.value())
                     .with_label(
                         std::get<std::string>(_args[0]));
             }
         }},
        {"with_tag",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(
                 _args.size() == 2 &&
                 std::holds_alternative<std::string>(
                     _args[0]) &&
                 std::holds_alternative<std::string>(_args[1]));
             assert(_what.has_value() &&
                    (std::holds_alternative<GQL::Vertices>(
                         _what.value()) ||
                     std::holds_alternative<GQL::Edges>(
                         _what.value())));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .with_tag(std::get<std::string>(_args[0]),
                               std::get<std::string>(_args[1]));
             }
             else
             {
                 return std::get<GQL::Edges>(_what.value())
                     .with_tag(std::get<std::string>(_args[0]),
                               std::get<std::string>(_args[1]));
             }
         }},
        {"with_id",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.size() == 1 &&
                    std::holds_alternative<std::string>(
                        _args[0]) &&
                    _what.has_value());
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .with_id(std::stoi(
                         std::get<std::string>(_args[0])));
             }
             else
             {
                 return std::get<GQL::Edges>(_what.value())
                     .with_id(std::stoi(
                         std::get<std::string>(_args[0])));
             }
         }},
        {"join",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.size() == 1 && _what.has_value());
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 assert(std::holds_alternative<GQL::Vertices>(
                     _args[0]));
                 return std::get<GQL::Vertices>(_what.value())
                     .join(std::get<GQL::Vertices>(_args[0]));
             }
             else
             {
                 assert(std::holds_alternative<GQL::Edges>(
                     _args[0]));
                 return std::get<GQL::Edges>(_what.value())
                     .join(std::get<GQL::Edges>(_args[0]));
             }
         }},
        {"intersection",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.size() == 1 && _what.has_value());
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 assert(std::holds_alternative<GQL::Vertices>(
                     _args[0]));
                 return std::get<GQL::Vertices>(_what.value())
                     .intersection(
                         std::get<GQL::Vertices>(_args[0]));
             }
             else
             {
                 assert(std::holds_alternative<GQL::Edges>(
                     _args[0]));
                 return std::get<GQL::Edges>(_what.value())
                     .intersection(
                         std::get<GQL::Edges>(_args[0]));
             }
         }},
        {"complement",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.size() == 1 && _what.has_value());
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 assert(std::holds_alternative<GQL::Vertices>(
                     _args[0]));
                 return std::get<GQL::Vertices>(_what.value())
                     .complement(
                         std::get<GQL::Vertices>(_args[0]));
             }
             else
             {
                 assert(std::holds_alternative<GQL::Edges>(
                     _args[0]));
                 return std::get<GQL::Edges>(_what.value())
                     .complement(
                         std::get<GQL::Edges>(_args[0]));
             }
         }},
        {"label",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.size() == 0 || _args.size() == 1);
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 if (_args.empty())
                 {
                     return std::get<GQL::Vertices>(
                                _what.value())
                         .label();
                 }
                 else
                 {
                     return std::get<GQL::Vertices>(
                                _what.value())
                         .label(
                             std::get<std::string>(_args[0]));
                 }
             }
             else
             {
                 if (_args.empty())
                 {
                     return std::get<GQL::Edges>(_what.value())
                         .label();
                 }
                 else
                 {
                     return std::get<GQL::Edges>(_what.value())
                         .label(
                             std::get<std::string>(_args[0]));
                 }
             }
         }},
        {"tag",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() && _args.size() > 0 &&
                    _args.size() < 3);
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 if (_args.size() == 1)
                 {
                     assert(std::holds_alternative<std::string>(
                         _args[0]));
                     return std::get<GQL::Vertices>(
                                _what.value())
                         .tag(std::get<std::string>(_args[0]));
                 }
                 else
                 {
                     assert(std::holds_alternative<std::string>(
                                _args[0]) &&
                            std::holds_alternative<std::string>(
                                _args[1]));
                     return std::get<GQL::Vertices>(
                                _what.value())
                         .tag(std::get<std::string>(_args[0]),
                              std::get<std::string>(_args[1]));
                 }
             }
             else
             {
                 if (_args.size() == 1)
                 {
                     assert(std::holds_alternative<std::string>(
                         _args[0]));
                     return std::get<GQL::Edges>(_what.value())
                         .tag(std::get<std::string>(_args[0]));
                 }
                 else
                 {
                     assert(std::holds_alternative<std::string>(
                                _args[0]) &&
                            std::holds_alternative<std::string>(
                                _args[1]));
                     return std::get<GQL::Edges>(_what.value())
                         .tag(std::get<std::string>(_args[0]),
                              std::get<std::string>(_args[1]));
                 }
             }
         }},
        {"id",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.empty() && _what.has_value());
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .id();
             }
             else
             {
                 return std::get<GQL::Edges>(_what.value())
                     .id();
             }
         }},
        {"select",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.size() == 1 &&
                    std::holds_alternative<std::string>(
                        _args[0]) &&
                    _what.has_value());
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .select(std::get<std::string>(_args[0]));
             }
             else
             {
                 return std::get<GQL::Edges>(_what.value())
                     .select(std::get<std::string>(_args[0]));
             }
         }},
        {"erase",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_args.empty() && _what.has_value());
             assert(std::holds_alternative<GQL::Vertices>(
                        _what.value()) ||
                    std::holds_alternative<GQL::Edges>(
                        _what.value()));
             if (std::holds_alternative<GQL::Vertices>(
                     _what.value()))
             {
                 std::get<GQL::Vertices>(_what.value()).erase();
                 return {};
             }
             else
             {
                 std::get<GQL::Edges>(_what.value()).erase();
                 return {};
             }
         }},
        {"traverse",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() &&
                    std::holds_alternative<GQL::Vertices>(
                        _what.value()) &&
                    _args.size() < 3);
             if (_args.size() == 0)
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .traverse();
             }
             else if (_args.size() == 1)
             {
                 assert(std::holds_alternative<std::string>(
                     _args[0]));
                 return std::get<GQL::Vertices>(_what.value())
                     .traverse(std::get<std::string>(_args[0]));
             }
             else
             {
                 assert(std::holds_alternative<std::string>(
                            _args[0]) &&
                        std::holds_alternative<std::string>(
                            _args[1]));
                 return std::get<GQL::Vertices>(_what.value())
                     .traverse(std::get<std::string>(_args[0]),
                               std::get<std::string>(_args[1]));
             }
         }},
        {"r_traverse",
         [&](OptVarType _what, std::vector<VarType> _args)
             -> OptVarType { return {}; }},
        {"add_edge",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() &&
                    std::holds_alternative<GQL::Vertices>(
                        _what.value()) &&
                    _args.size() == 1 &&
                    std::holds_alternative<GQL::Vertices>(
                        _args[0]));
             return std::get<GQL::Vertices>(_what.value())
                 .add_edge(std::get<GQL::Vertices>(_args[0]));
         }},
        {"in",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() &&
                    std::holds_alternative<GQL::Vertices>(
                        _what.value()) &&
                    _args.empty());
             return std::get<GQL::Vertices>(_what.value()).in();
         }},
        {"out",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() &&
                    std::holds_alternative<GQL::Vertices>(
                        _what.value()) &&
                    _args.empty());
             return std::get<GQL::Vertices>(_what.value())
                 .out();
         }},
        {"with_in_degree",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(
                 _what.has_value() &&
                 std::holds_alternative<GQL::Vertices>(
                     _what.value()) &&
                 _args.size() > 0 && _args.size() < 3 &&
                 std::holds_alternative<std::string>(_args[0]));
             assert(
                 _args.size() == 1 ||
                 std::holds_alternative<std::string>(_args[1]));
             if (_args.size() == 1)
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .with_in_degree(std::stoi(
                         std::get<std::string>(_args[0])));
             }
             else
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .with_in_degree(
                         std::stoi(
                             std::get<std::string>(_args[0])),
                         std::get<std::string>(_args[1]));
             }
         }},
        {"with_out_degree",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(
                 _what.has_value() &&
                 std::holds_alternative<GQL::Vertices>(
                     _what.value()) &&
                 _args.size() > 0 && _args.size() < 3 &&
                 std::holds_alternative<std::string>(_args[0]));
             assert(
                 _args.size() == 1 ||
                 std::holds_alternative<std::string>(_args[1]));
             if (_args.size() == 1)
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .with_out_degree(std::stoi(
                         std::get<std::string>(_args[0])));
             }
             else
             {
                 return std::get<GQL::Vertices>(_what.value())
                     .with_out_degree(
                         std::stoi(
                             std::get<std::string>(_args[0])),
                         std::get<std::string>(_args[1]));
             }
         }},
        {"with_source",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() &&
                    std::holds_alternative<GQL::Edges>(
                        _what.value()) &&
                    _args.size() == 1 &&
                    std::holds_alternative<GQL::Vertices>(
                        _args[0]));
             return std::get<GQL::Edges>(_what.value())
                 .with_source(
                     std::get<GQL::Vertices>(_args[0]));
         }},
        {"with_target",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() &&
                    std::holds_alternative<GQL::Edges>(
                        _what.value()) &&
                    _args.size() == 1 &&
                    std::holds_alternative<GQL::Vertices>(
                        _args[0]));
             return std::get<GQL::Edges>(_what.value())
                 .with_target(
                     std::get<GQL::Vertices>(_args[0]));
         }},
        {"source",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() &&
                    std::holds_alternative<GQL::Edges>(
                        _what.value()) &&
                    _args.empty());
             return std::get<GQL::Edges>(_what.value())
                 .source();
         }},
        {"target",
         [&](OptVarType _what,
             std::vector<VarType> _args) -> OptVarType {
             assert(_what.has_value() &&
                    std::holds_alternative<GQL::Edges>(
                        _what.value()) &&
                    _args.empty());
             return std::get<GQL::Edges>(_what.value())
                 .target();
         }}};

    // Process instructions (recursive descent)
    const std::function<OptVarType()> do_stmt =
        [&]() -> OptVarType {
        OptVarType out;
        std::string name;
        std::vector<VarType> args;

        // Do one chunk (name followed by optional arguments).
        // Then, if "." follows, continue. Else, exit.
        do
        {
            if (inp[pos] == ".")
            {
                ++pos;
            }

            // Name
            name = inp[pos];

            // Function call
            if (pos < inp.size() && inp[pos + 1] == "(")
            {
                // Collect arguments
                ++pos;
                args.clear();
                if (inp[pos + 1] != ")")
                {
                    do
                    {
                        ++pos;
                        const auto r = do_stmt();
                        if (r.has_value())
                        {
                            args.push_back(r.value());
                        }
                    } while (inp[pos] == ",");
                }
                else
                {
                    ++pos;
                }
                assert(inp[pos] == ")");
                const auto op = operations.at(name);
                out = op(out, args);
            }

            // String literal
            else if (name.front() == '\'' ||
                     name.front() == '"')
            {
                out = inp[pos].substr(1, inp[pos].size() - 2);
            }

            // Var name
            else
            {
                out = variables.at(name);
            }

            ++pos;
            // `pos` now points to next item
        } while (pos < inp.size() && inp[pos] == ".");

        return out;
    };

    // Call the parser
    while (pos < inp.size() && is_running)
    {
        do_stmt();
        assert(inp[pos] == ";");
        ++pos;
    }

    // Dump variables
    for (const auto &p : variables)
    {
        std::cout << "Variable `" << p.first << "`:\n";
        const auto val = p.second;
        if (std::holds_alternative<std::string>(val))
        {
            std::cout << '"' << std::get<std::string>(val)
                      << '"';
        }
        else if (std::holds_alternative<GQL::Result>(val))
        {
            std::cout << std::get<GQL::Result>(val);
        }
        else if (std::holds_alternative<GQL::Vertices>(val))
        {
            std::cout << "<Vertices object>";
        }
        else // GQL::Edges
        {
            std::cout << "<Edges object>";
        }

        std::cout << '\n';
    }

    return 0;
}

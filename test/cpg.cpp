/*
CPG = AST + CFG + PDG
From another project I'm working on that could use a better GDBM
*/

#include <cassert>
#include <format>
#include <gql.hpp>
#include <iostream>
#include <string>

const static std::string stmt_v = "STATEMENT",
                         pred_v = "PREDICATE", ast_v = "AST",
                         data_v = "DATA", ast_e = "AST",
                         eps_e = "EPSILON", t_e = "TRUE",
                         f_e = "FALSE", d_e = "DATA",
                         is_cfg = std::format(
                             "label = '{}' OR label = '{}'",
                             pred_v, stmt_v);

int main()
{
    /*
    From source code:
    ```py
    1| a = 5
    2| b = 6
    3| if a < 10:
    4|     a = b * a
    5| print(a)
    ```

    CPG of above source code:
                           / --[F]-------------- \
                           |                     v
    [1] -[e]-> [2] -[e]-> <3> -[T]-> [4] -[e]-> [5]
     |          |          |          |          |
     =          =          <          =        print
    / \        / \        / \        / \         |
    a  5      b   6      a   10     a   =        a
                                       / \
                                      a   b
     |          |          |          |          |
     v          v          v          v          v
    (a)        (b)        (a)        (a)        (a)
     |          |         ^||        ^^|        ^^
     |          |         |||        || \______/ |
     |           \________|||_______/ |          |
      \__________________/ | \_______/           |
                            \___________________/
    */

    GQL g("cpg.db", true);

    // Note: In the real implementation this is all automated

    // 1| a = 5
    {
        auto s = g.add_vertex().label(stmt_v).tag("line", "1");
        auto e = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "1")
                     .tag("str", "=");
        auto a = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "1")
                     .tag("str", "a");
        auto c = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "1")
                     .tag("str", "5");
        s.add_edge(e).label(ast_e);
        e.add_edge(a.join(c)).label(ast_e);
    }

    // 2| b = 6
    {
        auto s = g.add_vertex().label(stmt_v).tag("line", "2");
        auto e = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "2")
                     .tag("str", "=");
        auto b = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "2")
                     .tag("str", "b");
        auto c = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "2")
                     .tag("str", "6");
        s.add_edge(e).label(ast_e);
        e.add_edge(b.join(c)).label(ast_e);
    }

    // 3| a < 10
    {
        auto s = g.add_vertex().label(pred_v).tag("line", "3");
        auto l = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "3")
                     .tag("str", "<");
        auto a = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "3")
                     .tag("str", "a");
        auto c = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "3")
                     .tag("str", "10");
        s.add_edge(l).label(ast_e);
        l.add_edge(a.join(c)).label(ast_e);
    }

    // 4| a = b * a
    {
        auto s = g.add_vertex().label(stmt_v).tag("line", "4");

        auto e = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "4")
                     .tag("str", "=");
        auto a1 = g.add_vertex()
                      .label(ast_v)
                      .tag("line", "4")
                      .tag("str", "a");
        auto t = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "4")
                     .tag("str", "*");
        auto b = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "4")
                     .tag("str", "b");
        auto a2 = g.add_vertex()
                      .label(ast_v)
                      .tag("line", "4")
                      .tag("str", "a");
        s.add_edge(e).label(ast_e);
        e.add_edge(a1.join(t)).label(ast_e);
        t.add_edge(b.join(a2)).label(ast_e);
    }

    // 5| print(a)
    {
        auto s = g.add_vertex().label(stmt_v).tag("line", "5");

        auto p = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "5")
                     .tag("str", "print");
        auto a = g.add_vertex()
                     .label(ast_v)
                     .tag("line", "5")
                     .tag("str", "a");
        s.add_edge(p).label(ast_e).target().add_edge(a).label(
            ast_e);
    }

    // CFG stuff
    g.v(is_cfg)
        .with_tag("line", "1")
        .add_edge(g.v(is_cfg).with_tag("line", "2"))
        .label(eps_e);
    g.v(is_cfg)
        .with_tag("line", "2")
        .add_edge(g.v(is_cfg).with_tag("line", "3"))
        .label(eps_e);
    g.v(is_cfg)
        .with_tag("line", "3")
        .add_edge(g.v(is_cfg).with_tag("line", "5"))
        .label(f_e);
    g.v(is_cfg)
        .with_tag("line", "3")
        .add_edge(g.v(is_cfg).with_tag("line", "4"))
        .label(t_e);
    g.v(is_cfg)
        .with_tag("line", "4")
        .add_edge(g.v(is_cfg).with_tag("line", "5"))
        .label(eps_e);

    // Now PDG stuff
    auto a1 = g.v()
                  .with_tag("line", "1")
                  .where(is_cfg)
                  .add_edge(g.add_vertex().label(data_v).tag(
                      "var", "a"))
                  .label(d_e)
                  .source();
    auto b1 = g.v()
                  .with_tag("line", "2")
                  .where(is_cfg)
                  .add_edge(g.add_vertex().label(data_v).tag(
                      "var", "b"))
                  .label(d_e)
                  .source();
    auto a2 = g.v()
                  .with_tag("line", "3")
                  .where(is_cfg)
                  .add_edge(g.add_vertex().label(data_v).tag(
                      "var", "a"))
                  .label(d_e)
                  .source();
    auto a3 = g.v()
                  .with_tag("line", "4")
                  .where(is_cfg)
                  .add_edge(g.add_vertex().label(data_v).tag(
                      "var", "a"))
                  .label(d_e)
                  .source();
    auto a4 = g.v()
                  .with_tag("line", "5")
                  .where(is_cfg)
                  .add_edge(g.add_vertex().label(data_v).tag(
                      "var", "a"))
                  .label(d_e)
                  .source();

    a1.add_edge(a2).label(d_e);
    b1.add_edge(a3).label(d_e);
    a2.add_edge(a3).label(d_e);
    a2.add_edge(a4).label(d_e);
    a3.add_edge(a4).label(d_e);

    g.graphviz("foo.dot");

    /*
    Definitions:
    \[
    \begin{aligned}
        \texttt{PARENTS}, \texttt{CHILDREN}, \texttt{ANCESTORS},
            \texttt{DESCENDENTS}, \texttt{COMPONENT}
            : V -> 2^V \\
        E := V \times V \\
        \texttt{PARENTS}(v) =
            \{ u : (u, v) \in E \land
            \sigma((u, v)) = \texttt{DATA} \} \\
        \texttt{CHILDREN}(u) =
            \{ v : (u, v) \in E \land
            \sigma((u, v)) = \texttt{DATA} \} \\
        \texttt{ANCESTORS}(u) =
            \texttt{PARENTS}(u) \cup \{ \texttt{ANCESTORS}(v)
            : v \in \texttt{PARENTS}(u) \} \\
        \texttt{DESCENDENTS}(u) =
            \texttt{CHILDREN}(u) \cup \{ \texttt{DESCENDENTS}(v)
            : v \in \texttt{CHILDREN}(u) \} \\
        \texttt{COMPONENT}(u) = \texttt{ANCESTORS}(u) \cup
            \texttt{DESCENDENTS}(u) \\
    \end{aligned}
    \]
    */

    // Query definitions
    auto parents = [](GQL::Vertices _what) {
        return _what            // The nodes we were givin
            .where(is_cfg)      // Use only DFA nodes
            .out()              // Edges leading out
            .with_label(d_e)    // Only PDA edges
            .target()           // PDA nodes modified by src
            .in()               // Edges leading into
            .with_label(d_e)    // Only PDA edges
            .source()           // Sources of these edges
            .with_label(data_v) // Only data sources
            .in()               // Edges leading into source
            .with_label(d_e)    // Only PDA edges still
            .source()           // Source of this final edge
            .where(is_cfg);     // Only DFA sources
    };
    auto children = [](GQL::Vertices _what) {
        return _what            // The nodes we were givin
            .where(is_cfg)      // Use only DFA nodes
            .out()              // Edges leading out
            .with_label(d_e)    // Only PDA edges
            .target()           // PDA nodes modified by src
            .out()              // Edges leading out of
            .with_label(d_e)    // Only PDA edges
            .target()           // Targets of these edges
            .with_label(data_v) // Only data sources
            .in()               // Edges leading into source
            .with_label(d_e)    // Only PDA edges still
            .source()           // Source of this final edge
            .where(is_cfg);     // Only DFA sources
    };

    auto ancestors = [](GQL::Vertices _what) {
        // Enter PDG
        auto vars =
            _what.where(is_cfg).out().with_label(d_e).target();

        // Traverse
        auto travs = vars.r_traverse("label = " + data_v,
                                     "label = " + d_e);

        // Exit to CFG
        return travs.in().with_label(d_e).source().where(
            is_cfg);
    };
    auto descendents = [](GQL::Vertices _what) {
        // Enter PDG
        auto vars =
            _what.where(is_cfg).out().with_label(d_e).target();

        // Traverse
        auto travs = vars.traverse("label = " + data_v,
                                   "label = " + d_e);

        // Exit to CFG
        return travs.in().with_label(d_e).source().where(
            is_cfg);
    };

    auto component = [&](GQL::Vertices &_what) {
        return ancestors(_what).join(descendents(_what));
    };

    /*
    Example queries:

    PARENTS([5]) => { [1], [4] }
    ANCESTORS([5]) => { [1], [4], [2], <3> }
    CHILDREN([1]) => { <3>, [5] }
    DESCENDENTS([1]) => { <3>, [5], [4] }
    */
    auto p = parents(g.v().with_tag("line", "5")).tag("line");

    std::cout << "All nodes:\n"
              << g.v().select() << "All edges:\n"
              << g.e().select() << '\n';
    g.graphviz("foo.dot");
    assert(system("dot -Tpng foo.dot -o foo.png") == 0);

    std::cout
        << "1\n"
        << g.v().with_tag("line", "5").select() << "2\n"
        << g.v().with_tag("line", "5").where(is_cfg).select()
        << "3\n"
        << g.v()
               .with_tag("line", "5")
               .where(is_cfg)
               .out()
               .select()
        << "4\n"
        << g.v()
               .with_tag("line", "5")
               .where(is_cfg)
               .out()
               .with_label(d_e)
               .select()
        << "5\n"
        << g.v()
               .with_tag("line", "5")
               .where(is_cfg)
               .out()
               .with_label(d_e)
               .target()
               .select()
        << "6\n"
        << g.v()
               .with_tag("line", "5")
               .where(is_cfg)
               .out()
               .with_label(d_e)
               .target()
               .in()
               .select()
        << "7\n"
        << g.v()
               .with_tag("line", "5")
               .where(is_cfg)
               .out()
               .with_label(d_e)
               .target()
               .in()
               .with_label(d_e)
               .select()
        << "8\n"
        << g.v()
               .with_tag("line", "5")
               .where(is_cfg)
               .out()
               .with_label(d_e)
               .target()
               .in()
               .with_label(d_e)
               .source()
               .select();

    /*
        1 | return _what            // The nodes we were given
        2 |     .where(is_dfa)      // Use only DFA nodes
        3 |     .out()              // Edges leading out
        4 |     .with_label(d_e)    // Only PDA edges
        5 |     .target()           // PDA nodes modified by src
        6 |     .in()               // Edges leading into
        7 |     .with_label(d_e)    // Only PDA edges
        8 |     .source()           // Sources of these edges
        9 |     .with_label(data_v) // Only data sources
        10|     .in()               // Edges leading into source
        11|     .with_label(d_e)    // Only PDA edges still
        12|     .source()           // Source of this final edge
        13|     .where(is_dfa);     // Only DFA sources
    */

    // assert(p["line"][0] == "1" && p["line"][1] == "4");

    return 0;
}

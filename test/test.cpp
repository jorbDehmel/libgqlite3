/*
Janky tests
*/

#if !(__has_include(<gql.hpp>))
#error "Cannot test before installation"
#endif

#include <cstdint>
#include <gql.hpp>
#include <iostream>
#include <string>
#include <sys/types.h>

int main()
{
    GQL g("foo.db", true);

    auto a = stoi(g.add_vertex(1).id()[0][0]);
    auto b = stoi(g.add_vertex(2).id()[0][0]);
    g.add_edge(a, b);

    std::cout << "All nodes:\n"
              << g.v().id() << "Nodes with id = 1:\n"
              << g.v().where("id = 1").id()
              << "Edges leading from 1:\n"
              << g.v().where("id = 1").out().id()
              << "Nodes which lead from 1 by an edge:\n"
              << g.v().where("id = 1").out().target().id()
              << "Edges:\n"
              << g.e().id();

    g.v().erase();
    std::cout << "After erasure:\n" << g.v().id();

    // Demo: Which numbers divide which?
    std::cout << "In division example.\n";
    for (uint64_t i = 2; i < 102; ++i)
    {
        g.add_vertex(i).label(std::to_string(i));

        for (uint64_t j = 2; j < i; ++j)
        {
            if (i % j == 0)
            {
                g.add_edge(j, i).label("");
            }
        }
    }

    // Ensure no self-loops remain
    g.e().where("source = target").erase();

    auto all = g.v().id();
    auto id_index = all.index_of("id");

    for (uint64_t i = 0; i < all.size(); ++i)
    {
        auto cur =
            g.v(std::format("id = {}", all[i][id_index]));

        if (cur.in().id().empty())
        {
            cur.tag("prime", "true");
        }
    }

    g.graphviz("division.dot");

    std::cout << "Primes:\n"
              << g.v().with_tag("prime", "true").label();

    return 0;
}

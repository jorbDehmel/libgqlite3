/*
Maintains a GQL db which is similar to a makefile.
*/

#include "../src/gql.hpp"

#include <initializer_list>
#include <iostream>
#include <list>
#include <string>

class Maker
{
  public:
    Maker() : g("maker.db", true)
    {
    }

    void add_rule(
        const std::string &_target,
        const std::initializer_list<std::string> &_needs)
    {
        // If not already present, add
        if (g.v().with_label(_target).id().empty())
        {
            g.add_vertex().label(_target);
        }

        // Add dep links
        for (const auto &contributor : _needs)
        {
            // Ensure existence
            if (g.v().with_label(contributor).id().empty())
            {
                g.add_vertex().label(contributor);
            }

            // Add edge
            g.v()
                .with_label(contributor)
                .add_edge(g.v().with_label(_target));
        }
    }

    std::list<std::string> produce(const std::string &_target)
    {
        std::list<std::string> out;

        while (!g.v().with_label(_target).id().empty())
        {
            // Get all nodes whose in-degrees
            // are zero
            auto res = g.v().with_in_degree(0);

            // Get labels
            auto labels = res.label();

            // Append labels
            auto label_vec = labels["label"];
            for (const auto &item : label_vec)
            {
                out.push_back(item);
            }

            // Erase
            res.erase();
        }

        return out;
    }

    GQL g;
};

int main()
{
    Maker m;

    m.add_rule("main.out", {"main.o", "lib.o", "lib.so"});
    m.add_rule("main.o", {"main.cpp"});
    m.add_rule("lib.o", {"lib.cpp"});

    auto order = m.produce("main.out");

    std::cout << "Valid build order:\n";
    for (const auto &item : order)
    {
        std::cout << item << ' ';
    }
    std::cout << '\n';

    return 0;
}

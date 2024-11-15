/*
Basic recursive traversal tests for GQL
*/

#include "../src/gql.hpp"

#include <cassert>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>

void assert_eq(const GQL::Result &_obs,
               const std::initializer_list<std::string> &_exp)
{
    bool did_match = true;

    // Checks
    if (_obs.size() != _exp.size())
    {
        did_match = false;
    }
    else
    {
        auto obs_it = _obs.body.begin();
        auto exp_it = _exp.begin();
        for (;
             obs_it != _obs.body.end() && exp_it != _exp.end();
             ++obs_it, ++exp_it)
        {
            if ((*obs_it)[0] != *exp_it)
            {
                did_match = false;
                break;
            }
        }
    }

    if (!did_match)
    {
        std::cout << "Expected: ";
        for (const auto &item : _exp)
        {
            std::cout << item << ' ';
        }
        std::cout << "\nObserved: ";
        for (const auto &item : _obs.body)
        {
            std::cout << item[0] << ' ';
        }
        std::cout << '\n';

        throw std::runtime_error("fail");
    }
}

int main()
{
    GQL g("trav.db", true);

    for (uint64_t i = 1; i <= 5; ++i)
    {
        g.add_vertex(i).label(std::to_string(i));
    }

    // 1 -> 2 -> 3
    // 4 -> 5
    g.add_edge(1, 2);
    g.add_edge(2, 3);
    g.add_edge(4, 5);

    assert_eq(g.v("id = 1").traverse().id(), {"1", "2", "3"});
    assert_eq(g.v("id = 2").traverse().id(), {"2", "3"});

    // 1 -> 2 -> 3
    //      + -> 4 -> 5
    g.add_edge(2, 4);

    assert_eq(g.v("id = 1").traverse().id(),
              {"1", "2", "3", "4", "5"});
    assert_eq(g.v("id = 5").r_traverse().id(),
              {"1", "2", "4", "5"});
    assert_eq(g.v("id = 1").traverse("nodes.id != 4").id(),
              {"1", "2", "3"});

    g.e()
        .where("source = 2")
        .where("target = 4")
        .label("DUMMY");

    assert_eq(g.v("id = 1")
                  .traverse("1", "edges.label != 'DUMMY'")
                  .id(),
              {"1", "2", "3"});

    return 0;
}

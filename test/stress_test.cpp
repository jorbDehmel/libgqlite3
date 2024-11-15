/*
Tests the handling of very large graphs. This is an n^2
algorithm on a large n. Time AND db space should be taken into
account.
*/

#include "../src/gql.hpp"
#include <cassert>
#include <format>
#include <iostream>

#define START 1
#define END 10'000ULL

constexpr inline const uint64_t n_divisors(const uint64_t what)
{
    uint64_t out = 0;
    for (uint64_t i = 1; i * i <= what; ++i)
    {
        if (what % i == 0)
        {
            ++out;
        }
    }
    return out;
}

int main()
{
    uint64_t start, stop, stop2;
    uint64_t elapsed_s, verification_elapsed_s;

    GQL g("/tmp/gql_stress_test.db", true);

    // Start timer
    start = time(NULL);

    // Iterate over numbers in range
    for (uint64_t cur = START; cur < END; ++cur)
    {
        if (cur % 5'000 == 0)
        {
            std::cout << "Processing " << cur << " of " << END
                      << " ("
                      << 100.0 * (double)cur / (double)END
                      << "%)\n";
            g.commit();
        }

        // Add the new largest number
        g.add_vertex(cur).label(std::to_string(cur));

        // Add divisibility edges
        g.v()
            .where("id * id <= " + std::to_string(cur))
            .where("MOD(" + std::to_string(cur) + ", id) = 0")
            .add_edge(
                g.v().where("id = " + std::to_string(cur)));
    }

    // Stop timer
    stop = time(NULL);
    elapsed_s = stop - start;

    // Print number of divisors for each node
    const GQL::Result res = g.v().in_degree();
    for (const auto &p : res)
    {
        // Assert correctness
        const auto obs = std::stoull(p[1]);
        const auto exp = n_divisors(std::stoi(p[0]));
        assert(obs == exp);
    }

    stop2 = time(NULL);
    verification_elapsed_s = stop2 - stop;

    // Print stats
    std::cout << "\nFound the factor counts of " << END - START
              << " numbers, leading to " << g.sql_call_counter
              << " DB calls in " << elapsed_s << " s (versus "
              << verification_elapsed_s
              << " s verification time). Final "
              << "node count: " << g.v().id().size() << " final"
              << " edge count: " << g.e().id().size() << '\n'
              << "Found " << g.v().with_in_degree(1).id().size()
              << " primes.\nAverage ms / DB call: "
              << 1'000.0 * (double)elapsed_s /
                     (double)g.sql_call_counter
              << '\n';

    return 0;
}

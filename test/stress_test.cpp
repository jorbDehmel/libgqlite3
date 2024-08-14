/*
Tests the handling of very large graphs. This is an n^2
algorithm on a large n. Time AND db space should be taken into
account.

Note: We cannot use <chrono>, as it forces you to include
std::format, which messes up our backup build target.

Using std::format   313 s
Using custom format 304 s
(basically the same)
*/

#include <gql.hpp>
#include <iostream>

#define START 2
#define END 100'000ULL

int main()
{
    uint64_t start, stop;
    uint64_t elapsed_s;

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
            .where(std::format("id * id <= {}", cur))
            .where(std::format("MOD({}, id) = 0", cur, cur))
            .add_edge(g.v().where(std::format("id = {}", cur)));
    }

    // Erase self-loops
    g.e().where("source = target").erase();

    // Mark primes
    g.v().with_in_degree(0).tag("prime", "true");

    // Stop timer
    stop = time(NULL);
    elapsed_s = stop - start;

    // Print primes
    auto primes = g.v().with_tag("prime", "true").id()["id"];
    for (const auto &p : primes)
    {
        std::cout << p << ' ';
    }

    // Print stats
    std::cout << "\nChecked " << END - START << " numbers for "
              << "primality, leading to " << g.sql_call_counter
              << " DB calls in " << elapsed_s << " s. Final "
              << "node count: " << g.v().id().size() << " final"
              << " edge count: " << g.e().id().size() << '\n'
              << "Found "
              << g.v().with_tag("prime", "true").id().size()
              << " primes.\nAverage ms / DB call: "
              << 1'000.0 * (double)elapsed_s /
                     (double)g.sql_call_counter
              << '\n';

    return 0;
}

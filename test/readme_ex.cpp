// main.cpp

#include "../src/gql.hpp"
#include <iostream>

int main()
{
    // Create `foo.db` and ensure it is empty
    GQL g("foo.db", true);

    // Add a vertex and return a SQL result representing it
    auto new_id = g.add_vertex().id();

    // Decode the id from the SQL result
    uint64_t from = new_id.front();

    // Add a vertex with identifier 123
    g.add_vertex(123);

    // Add an edge from the first node to the second
    g.add_edge(from, 123);

    auto to_print =
        g          // From the database `g`
            .v()   // Select all nodes
            .id(); // Get the id of the selected nodes

    std::cout << "All nodes: ";
    for (const auto &item : to_print)
    {
        std::cout << item << ' ';
    }

    to_print = g             // From the database `g`
                   .e()      // Select all edges
                   .target() // Select the target nodes
                   .id();    // Get the id of the target nodes

    std::cout << "\nAll edges: ";
    for (const auto &item : to_print)
    {
        std::cout << item << ' ';
    }
    std::cout << '\n';

    // For each string in a list
    for (auto item : {"foo", "fizz", "buzz"})
    {
        g        // From the database `g`
            .v() // Get all nodes
            .where([](auto v) {
                return v.id().front() < 100;
            })            // Select those with id < 100
            .label(item); // Set the label of these nodes
    }

    return 0;
}

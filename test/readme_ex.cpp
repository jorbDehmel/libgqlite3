// main.cpp

#include "../src/gql.hpp"
#include <iostream>

int main()
{
    // Create `foo.db` and ensure it is empty
    GQL g("foo.db", true);

    // Add a vertex and return a SQL result representing it
    auto res = g.add_vertex().id();

    // Decode the id from the SQL result
    uint64_t from = stoi(res[0][0]);

    // Add a vertex with identifier 123
    g.add_vertex(123);

    // Add an edge from the first node to the second
    g.add_edge(from, 123);

    std::cout << g          // From the database `g`
                     .v()   // Select all nodes
                     .id(); // Get the id of the selected nodes

    std::cout << g             // From the database `g`
                     .e()      // Select all edges
                     .target() // Select the target nodes
                     .id();    // Get the id of the target nodes

    // For each string in a list
    for (auto item : {"foo", "fizz", "buzz"})
    {
        g                  // From the database `g`
            .v("id < 100") // Select all nodes with id < 100
            .label(item);  // Set the label of these nodes
    }

    return 0;
}

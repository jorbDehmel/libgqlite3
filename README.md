
# GQL (Graph SQLite3)
Jordan Dehmel, 2024

## Outline

A simple Graph DataBase Manager (GDBM) library for `C++` using
`libsqlite3`. Loosely based on `Gremlin`, but aiming to spin up
faster. No CLI is provided here (only `C++` lib), but one would
be semitrivial to implement.

## Requirements

This software assumes a POSIX-compliant environment. Do not
assume or expect it to work on Windows.

You must have the following packages installed:
- `C++` 20 or later **with `std::format`** (`clang` works well)
- `libsqlite3-dev` (the `C` headers, not just the CLI)

## Installation

1) Clone this repo locally
2) Navigate to this directory
3) From this directory, run `make install`

## Example

```cpp
// main.cpp

#include <gql.hpp>
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
        g                   // From the database `g`
            .v("id < 100")  // Select all nodes with id < 100
            .label(item);   // Set the label of these nodes
    }

    return 0;
}

```

```sh
clang++ -std=c++20 main.cpp -lsqlite3
```

## Licensing

This software uses the MIT license, which can be found in this
directory.

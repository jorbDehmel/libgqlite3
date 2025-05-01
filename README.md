
![Test Badge](https://github.com/jorbDehmel/gql/actions/workflows/ci-test.yml/badge.svg)

# GQL / `libgqlite3` (Graph SQLite3)
Jordan Dehmel, 2024 - 2025, MIT License

## Outline

A simple Graph DataBase Manager (GDBM) library for `C++` using
`libsqlite3`. Loosely based on `Gremlin`, but aiming to spin up
faster.

A GQL object instance represents exactly one database file and
exactly one property graph. The property graph $G$ managed is
defined as $G = (V, E, \lambda, \mu)$, where $V$ is the set of
vertices, $E$ is the set of edges,
$\lambda: (E \cup V) \to \Sigma$ associates a label from an
arbitrary label set $\Sigma$ to each edge and vertex, and
$\mu: (E \cup V) \times K \to S$ associates keys from arbitrary
key set $K$ to values from arbitrary value set $S$. In our case,
$\Sigma, K$ and $S$ are all subsets of `std::string`.

Pronounced "geek-uel", rhyming with "sequel".

## Requirements

This software assumes a POSIX-compliant environment. Do not
assume or expect it to work on Windows.

You must meet the following requirements:
- `C++` 11 or later (`g++ -std=c++20` is used for testing)
- `libsqlite3-dev` (the `C` headers, not just the CLI)

To check your environment, run `make check`.

**Warning:** This software relies on `std::format`, which is
only sometimes provided. If it does not exist on your machine,
this software is likely to run slower and be more bug-prone.

## Installation

1. Clone this repo locally
2. Navigate to this directory

### Use Case 1: `#include <gql.hpp>` or `#include <gqlite3.hpp>`

3. From this directory, run `make install`

### Use Case 2: `#include "./gql.hpp"` (casual local install)

3. Simply copy-paste the local file `./src/gql.hpp` anywhere you
    want to use it. This is allowed by the licensing without
    acknowledgment, and the entirety of the MIT license is
    included in the header file.

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
        g                  // From the database `g`
            .v("id < 100") // Select all nodes with id < 100
            .label(item);  // Set the label of these nodes
    }

    return 0;
}

```

Then compile as shown below (note the inclusion of `-lsqlite3`):
```sh
clang++ -std=c++20 main.cpp -lsqlite3
```

## Opening a Database Instance

The `GQL` object provides an interface to a SQLite3 database. To
open or create such a database, simply instantiate the object
with the desired file path.

```cpp
// Open an existing graph, or create it if it doesn't exist
GQL g("/path/to/database.db");

// The same as above, but erase all graph data after loading
GQL g2("/path/to/database.db", true);
```

A transaction is automatically opened upon instantiation, and
the database is committed upon object destruction. The object
also provides the `.commit()` method, which commits and opens a
new transaction.

**Note:** Since GQL is based on SQLite3, only one instance
should be opened upon a given database. Additionally, one GQL
object corresponds to exactly one property graph; Thus, to have
multiple property graphs you must maintain multiple `.db` files.

## Creating Graphs

The GQL handler object provides a few methods for adding nodes
and edges to the graph. Some basic ones are outlined in the code
below.

```cpp
GQL g("foo.db", true);  // Open and erase

g.add_vertex();         // Add a vertex with an unspecified ID
g.add_vertex(100);      // Add a vertex with the ID 100

auto node = g.add_vertex(); // Add a vertex and retrieve it

g.add_vertex().label("label");  // Create a node and label it

// Associate the key "fizz" w/  the value "buzz" on node
node.tag("fizz", "buzz");

// Add an edge from node to 100 w/ the label "some edge" and the
// key-value pair ("foo", "bar").
g.add_edge(node, 100).label("some edge").tag("foo", "bar");

```

If no file path is provided to the constructor, the graph will be
**memory-only** (provided that your local installation of
`libsqlite3` honors the `:memory:` keyword).

```cpp
// Memory-only: Not saved to disk
GQL g;
```

## Query Types

There are three basic query types: `GQL::Vertices`, which
represents a set of nodes on the graph, `GQL::Edges`, which
represents a set of edges on the graph, and `GQL::Result`, which
is a SQL result table composed of a vector of headers and a
table body. All queries on vertices or edges will return either
a subset of themselves, a subset of the opposite type (for
instance, calling `target()` on edges returns `Vertices`) or a
result table that cannot be queried.

**Note:** Queries are not evaluated until a `Result`-returning
or augmenting query is called.

```cpp
// No query is evaluated, since `GQL::v` returns a
// `GQL::Vertices` object
auto v_set = g.v();

// Still no evaluation, since `GQL::Vertices::where` returns a
// `GQL::Vertices` object
auto v_subset = v_set.where("label = 'foo'");

// *Now* the database is read, since `GQL::Vertices::id`
// returns a GQL::Result object.
auto ids_of_v_subset = v_subset.id();

// This also calls the database, since the label setting
// function is void.
v_subset.label("fizz");

```

## Basic Queries

**Note:** Assume that every terminal query is ordered by
ascending ID: Even traversals.

### Entering the Graph

A GQL instance has a few functions to access its nodes and
edges. For some GQL instance `g`, `g.v()` yields the set of all
vertices and `g.e()` yields the set of all edges.
`g.v("id = 1")` is the same as `g.v().where("id = 1")`, and
`g.e("source = 1")` is the same as `g.e().where("source = 1")`.

### From Vertices

For some `GQL::Vertices` object `v`, `v.where("...")` yields a
subset of `v` where the given condition holds.
`v.with_label("...")` is the same as `v.where("label = '...'")`,
and `v.with_tag("key", "value")` yields the subset of `v` where
the tag `"key"` is associated with the value `"value"`. For
another `GQL::Vertices` object `u`, `v.join(u)` yields all
vertices in `u`, `v`, or both. `v.intersection(u)` gives us the
set of vertices in both `u` and `v`. `v.complement(u)` yields
the set of vertices in `u` but not `v`. Thus,
`v.intersection(u).complement(v.join(u))` would yield
$\texttt{u} \oplus \texttt{v}$.

The `GQL::Vertices` object also provides several "terminal"
(database accessing) operations which yield `GQL::Result`
objects. For our earlier object `v`, `v.label()` will yield the
IDs of all selected nodes, along with their labels. Similarly,
`v.tag("key")` will yield the IDs along with the value
associated with the given key. `v.id()` will yield the IDs with
no extra information, and `v.select("...")` will perform the SQL
`SELECT id, ... FROM (selected)`.

The command `v.in()` will yield the set of all edges whose
targets are in our selection, and the command `v.out()` will
yield the set of all edges whose sources are in our selection.
The command `v.label("...")` will set the labels of all nodes
selected and `v.tag("fizz", "buzz")` will add a key-value pair
to them. `v.erase()` will erase the selected nodes from the
database.

The command `v.with_in_degree(n, condition)` selects all nodes
which have exactly `n` incoming edges for which `condition`
holds. If no condition is provided, it is treated as a
tautology. Similarly, `v.with_out_degree(n, condition)` selects
all nodes which have exactly `n` outgoing edges for which
`condition` holds.

Finally, vertices provide traversal functions. The traversal
function `v.traverse("edge case", "vertex case")` yields the
set of all vertices where `"vertex case"` holds and which are
reachable by edges for which `"edge case"` holds. This traversal
follows edges from source to target, but the `v.r_traverse`
(reverse traverse) function allows you to traverse from target
to source. If no cases are provided, the function assumes they
are tautologies.

### From edges

`GQL::Edges` objects provide the same definitions for `.where`,
`.with_label`, `.with_tag`, `.join`, `.intersection` and
`.complement` as `GQL::Vertices` does, except that they take and
return only edges objects. `.select`, `.label`, `.tag` and
`.erase` work the same as for `GQL::Vertices`. However, the
edges object contains no traversal or reverse traversal
functions, nor does it contain the `.in` and `.out` functions.
Instead, it has `.source` and `.target`, which yield the
vertices whose IDs are sources or targets in the edge set
respectively.

### From the Graph Manager

For a `GQL` object `g`, `g.v()` yields all vertices and `g.e()`
yields all edges. The method `g.commit()` commits the database
and opens a new transaction (a new transaction is opened at
instantiation and a final commit is written upon destruction).
The method `g.rollback()` will revert the database to before the
current transaction and begin a new one. Finally,
`g.graphviz("foo.dot")` will save a graphviz (dot)
representation of the graph to the specified file.

## Licensing

This software uses the MIT license, which can be found in this
directory or in the header file itself. No citation or
acknowledgment is needed or expected to use this software;
You are free to simply copy-paste the header into anything you
want.

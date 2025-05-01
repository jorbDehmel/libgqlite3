
# `gqlite3` CLI

Jordan Dehmel, 2025. Licensed under the MIT software license.

# About

This software is packaged as part of `libgqlite3`, which is a
header interface for `C++` licensed under the MIT license. The
`gqlite3` CLI allows users to use the `C++` library from the
command-line in much the same way `sqlite3` allows users access
to the `libsqlite3` `C` library.

# Installation and Use

From this directory (on `Linux`), run `make install`. After
installation, the CLI will be available as `gqlite3`. To exit
the CLI, run `q();`. For help, run `help();`.

# Command-line Options

These options can be passed to the executable.

 Flag    | Meaning
---------|------------------------------------------
 --help  | Show help text (this)
 --input | Execute from a script whose path follows

If no input file is provided, `stdin` will be used.

# Syntax

The syntax closely matches the syntax of the `C++` library,
except for variables.

## Variables

After any call, you can use `.as("...");` to save the results
as a variable named `...`. For instance, `GQL().as("G");`
creates a transient (in-memory) graph and saves it as `G`. To
use that variable, you say `G.whatever();`, where `whatever()`
is the query you want to perform. Note that, although `as` takes
a string literal, the name of the variable is not a string.
Because of this, it is easy to create variables names that can
never be accessed again (e.g. `.as("spaced name");`): Beware!

# Example

```gqlite3
// This is a comment
GQL("foo.db", "true", "false").as("G");

G.v()
  .erase();

G.add_vertex()
  .label("Alice");
G.add_vertex()
  .label("Bob")
  .tag("last name", "Crabbit");

G.v()
  .with_label("Alice")
  .add_edge(
    G.v()
      .with_label("Bob")
  )
  .label("is friends with");

G.v()
  .label()
  .as("names");

G.e()
  .label()
  .as("relations");

G.v().as("all the nodes");
G.e().as("all the edges");
```

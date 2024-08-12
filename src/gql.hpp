/*
A manager object which maintains a SQLite3 instance.
Based on work partially funded by Colorado Mesa University.
All functions are inline here.

This software is available via the MIT license.
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

// Assert compiler version
static_assert(__cplusplus > 2020'00L);

/*
GQL (Graph SQLite3) manager object

This is a database instance which manages a property graph
$G = (V, E, \lambda, \mu)$, where $V$ is the set of vertices,
$E$ is the set of edges, $\lambda: (E \cup V) \to \Sigma$
associates a label from an arbitrary label set $\Sigma$ to each
edge and vertex, and $\mu: (E \cup V) \times K \to S$ associates
keys from arbitrary key set $K$ to values from arbitrary value
set $S$. In our case, $\Sigma, K$ and $S$ are all subsets of
`std::string`.

GQL notation is loosely based on Gremlin. This software is
available via the MIT license.
*/
class GQL
{
  public:
    // Forward definitions
    class Result;
    class Vertices;
    class Edges;

    // Real definitions
    class Result
    {
      public:
        std::vector<std::vector<std::string>> body;
        std::vector<std::string> headers;

        // Get row
        inline const std::vector<std::string> &operator[](
            const size_t &_i) const
        {
            return body[_i];
        }

        // Get column (using other helper functions)
        inline const std::vector<std::string> operator[](
            const std::string &_col) const
        {
            std::vector<std::string> out;
            out.reserve(size());
            const auto ind = index_of(_col);

            for (int i = 0; i < size(); ++i)
            {
                out.push_back(operator[](i)[ind]);
            }

            return out;
        }

        // Get the number of items in this result
        inline size_t size() const noexcept
        {
            return body.size();
        }

        // Returns true iff the result is empty
        inline bool empty() const noexcept
        {
            return body.empty();
        }

        // Returns the index of the given column (given that it
        // exists)
        inline size_t index_of(const std::string &_what) const
        {
            auto it = std::find(headers.begin(), headers.end(),
                                _what);
            if (it == headers.end())
            {
                throw std::runtime_error(std::format(
                    "Header value '{}' is not present in "
                    "results.",
                    _what));
            }
            return std::distance(headers.begin(), it);
        }
    };

    class Vertices
    {
      protected:
        GQL *const owner;
        const std::string cmd;
        const uint64_t depth;

      public:
        Vertices(GQL *const _owner, const std::string &_cmd,
                 const uint64_t &_depth = 0)
            : owner(_owner), cmd(_cmd), depth(_depth)
        {
        }

        // Select all vertices where the given SQL statement
        // holds
        Vertices where(const std::string &_sql_statement);

        // Select all vertices with the given label
        Vertices with_label(const std::string &_label);

        // Select all vertices where the given tag key is
        // associated with the given value
        Vertices with_tag(const std::string &_key,
                          const std::string &_value);

        // Select all vertices which are in this, the passed
        // set, or both
        Vertices join(const Vertices &_other);

        // Select all vertices which are in both this and the
        // passed set
        Vertices intersection(const Vertices &_other);

        // Select all vertices which are in the universe set but
        // not this set
        Vertices complement(const Vertices &_universe);

        // Select all nodes reachable from this node set
        // according to a few recursive rules. This follows
        // edges FORWARDS. A new node is visited iff:
        // 1)   it is the target of an edge for which
        //      _where_edge holds and source is already in the
        //      traversal
        // 2)   _where_node holds for it
        Vertices traverse(const std::string &_where_node = "1",
                          const std::string &_where_edge = "1");

        // Select all nodes reachable from this node set
        // according to a few recursive rules. This follows
        // edges BACKWARDS. A new node is visited iff:
        // 1)   it is the source of an edge for which
        //      _where_edge holds and target is already in the
        //      traversal
        // 2)   _where_node holds for it
        Vertices r_traverse(
            const std::string &_where_node = "1",
            const std::string &_where_edge = "1");

        // Select the label from all nodes in this set
        Result label();

        // Select the value associated with the given key for
        // all vertices in this set
        Result tag(const std::string &_key);

        // Select the id of all vertices in this set
        Result id();

        // Return the results of the SQL `SELECT {} FROM (...)`,
        // where `{}` is the passed string and `...` is this set
        Result select(const std::string &_sql = "*");

        // Set the label associated with all the nodes in this
        // set
        void label(const std::string &_label);

        // For each vertex in this set, associate the tag `_key`
        // with the value `_value`
        void tag(const std::string &_key,
                 const std::string &_value);

        // Erase these nodes from the database
        void erase();

        // Add edges from each node in this set to each node
        // in the passed set (cartesian product).
        void add_edge(const Vertices &_to);

        // Get all edges leading into these vertices
        Edges in();

        // Get all edges leading out of these vertices
        Edges out();
    };

    class Edges
    {
      protected:
        GQL *const owner;
        const std::string cmd;
        const uint64_t depth;

      public:
        Edges(GQL *const _owner, const std::string &_cmd,
              const uint64_t &_depth = 0)
            : owner(_owner), cmd(_cmd), depth(_depth)
        {
        }

        // Select some subset of these edges according to a SQL
        // condition
        Edges where(const std::string &_sql_statement);

        // Select all edges which have the given label
        Edges with_label(const std::string &_label);

        // Get all edges which have the given key-value pair
        Edges with_tag(const std::string &_key,
                       const std::string &_value);

        // Return all values which are in either this set or
        // the passed one or both
        Edges join(const Edges &_other);

        // Return all values which are in both this set and the
        // passed one
        Edges intersection(const Edges &_other);

        // Return all values which are in the passed set but not
        // in this set
        Edges complement(const Edges &_universe);

        // Get the label of all edges in this set
        Result label();

        // Get the values associated with the given key
        Result tag(const std::string &_key);

        // Yield the ids of the edges in this set
        Result id();

        // Select something from these edges.
        // The form is: `SELECT {} FROM (...)`, with your
        // statement replacing `{}`.
        Result select(const std::string &_sql = "*");

        // Set the label of these edges
        void label(const std::string &_label);

        // Add a key-value pair to each of these edges.
        void tag(const std::string &_key,
                 const std::string &_value);

        // Erase these edges from the database.
        void erase();

        // Yield all vertices which are sources in this set of
        // edges.
        Vertices source();

        // Yield all vertices which are targets in this set of
        // edges.
        Vertices target();
    };

    /*
    Initialize from a file, or create it if it DNE. If _erase,
    purges all nodes and edges.
    */
    GQL(const std::string &_filepath,
        const bool &_erase = false);

    // Closes the database
    ~GQL();

    // Get all vertices
    Vertices v();

    // Get all edges
    Edges e();

    // Get all vertices where some SQL WHERE clause holds
    Vertices v(const std::string &_where);

    // Get all edges where some SQL WHERE clause holds
    Edges e(const std::string &_where);

    // Save a graphviz (.dot) representation of the database
    void graphviz(const std::string &_filepath);

    // Commit the database and open a new transaction.
    void commit();

    // Generate a new vertex and return a handle to it
    Vertices add_vertex();

    // Generate a new vertex with the given id and return a
    // handle to it
    Vertices add_vertex(const uint64_t &_id);

    // Generate an edge from the given source to the given
    // target
    Edges add_edge(const uint64_t &_source,
                   const uint64_t &_target);

  protected:
    // Execute the given sql and return the results
    Result sql(const std::string &_sql);

    sqlite3 *db = nullptr;
    uint64_t next_node_id = 1, next_edge_id = 1;
};

std::ostream &operator<<(std::ostream &_into,
                         const GQL::Result &_what);

////////////////////////////////////////////////////////////////

inline GQL::GQL(const std::string &_filepath,
                const bool &_erase)
{
    // Open database
    sqlite3_open(_filepath.c_str(), &db);

    // Initialize
    sql("CREATE TABLE IF NOT EXISTS nodes ("
        "id INTEGER NOT NULL, "
        "label TEXT DEFAULT '', "
        "tags TEXT DEFAULT '{}', "
        "PRIMARY KEY(id)"
        ");");
    sql("CREATE TABLE IF NOT EXISTS edges ("
        "id INTEGER NOT NULL, "
        "source INTEGER NOT NULL, "
        "target INTEGER NOT NULL, "
        "label TEXT DEFAULT '', "
        "tags TEXT DEFAULT '{}', "
        "PRIMARY KEY(id), "
        "FOREIGN KEY(source) REFERENCES nodes(id), "
        "FOREIGN KEY(target) REFERENCES nodes(id)"
        ");");

    if (_erase)
    {
        sql("DELETE FROM nodes;");
        sql("DELETE FROM edges;");
    }

    sql("BEGIN TRANSACTION;");
}

inline GQL::~GQL()
{
    sql("COMMIT;");
    sqlite3_close(db);
}

inline void GQL::commit()
{
    sql("COMMIT;");
    sql("BEGIN TRANSACTION;");
}

////////////////////////////////////////////////////////////////

inline GQL::Vertices GQL::Vertices::where(
    const std::string &_sql_statement)
{
    return GQL::Vertices(
        owner,
        std::format("SELECT * FROM ({}) WHERE {}", cmd,
                    _sql_statement),
        depth + 1);
}

inline GQL::Vertices GQL::Vertices::with_label(
    const std::string &_label)
{
    return GQL::Vertices(
        owner,
        std::format("SELECT * FROM ({}) s{} WHERE "
                    "s{}.label = '{}'",
                    cmd, depth, depth, _label),
        depth + 1);
}

inline GQL::Vertices GQL::Vertices::with_tag(
    const std::string &_key, const std::string &_value)
{
    return GQL::Vertices(
        owner,
        std::format("SELECT * FROM ({}) s{} WHERE "
                    "json_extract(s{}.tags, '$.{}') = '{}'",
                    cmd, depth, depth, _key, _value),
        depth + 1);
}

inline GQL::Vertices GQL::Vertices::join(
    const GQL::Vertices &_with)
{
    return GQL::Vertices(
        owner,
        std::format("SELECT * FROM ({}) UNION ({})", cmd,
                    _with.cmd),
        depth + 1);
}

inline GQL::Vertices GQL::Vertices::intersection(
    const GQL::Vertices &_with)
{
    return GQL::Vertices(
        owner,
        std::format("SELECT * FROM ({}) UNION ({})", cmd,
                    _with.cmd),
        depth + 1);
}

inline GQL::Vertices GQL::Vertices::complement(
    const GQL::Vertices &_universe)
{
    return GQL::Vertices(
        owner,
        std::format("SELECT * FROM ({}) WHERE * NOT IN ({})",
                    _universe.cmd, cmd),
        depth + 1);
}

inline GQL::Vertices GQL::Vertices::traverse(
    const std::string &_where_node,
    const std::string &_where_edge)
{
    return GQL::Vertices(
        owner,
        std::format("WITH RECURSIVE t AS ("
                    // Base case
                    "SELECT id FROM ({}) "
                    // Recursive call
                    "UNION "
                    "SELECT e.target AS id FROM t "
                    "JOIN (SELECT * FROM edges WHERE {}) e "
                    "ON t.id = e.source "
                    "JOIN (SELECT * FROM nodes WHERE {}) n "
                    "ON e.target = n.id "
                    "WHERE e.target IS NOT NULL) "
                    // End recursive call
                    "SELECT * FROM t JOIN nodes "
                    "ON t.id = nodes.id",
                    cmd, _where_edge, _where_node),
        depth + 1);
}

inline GQL::Vertices GQL::Vertices::r_traverse(
    const std::string &_where_node,
    const std::string &_where_edge)
{
    return GQL::Vertices(
        owner,
        std::format("WITH RECURSIVE t AS ("
                    // Base case
                    "SELECT id FROM ({}) "
                    // Recursive call
                    "UNION "
                    "SELECT e.source AS id "
                    "FROM t "
                    "JOIN (SELECT * FROM edges WHERE {}) e "
                    "ON t.id = e.target "
                    "JOIN (SELECT * FROM nodes WHERE {}) n "
                    "ON e.source = n.id "
                    "WHERE e.source IS NOT NULL) "
                    // End recursive call
                    "SELECT * FROM t JOIN nodes "
                    "ON t.id = nodes.id",
                    cmd, _where_edge, _where_node),
        depth + 1);
}

inline GQL::Result GQL::Vertices::label()
{
    return owner->sql(std::format(
        "SELECT s{}.id AS id, s{}.label AS label FROM "
        "({}) s{} ORDER BY id;",
        depth, depth, cmd, depth));
}

inline GQL::Result GQL::Vertices::tag(const std::string &_key)
{
    return owner->sql(
        std::format("SELECT s{}.id AS id, "
                    "json_extract(s{}.tags, '$.{}') AS {} "
                    "FROM ({}) s{} ORDER BY id;",
                    depth, depth, _key, _key, cmd, depth));
}

inline GQL::Result GQL::Vertices::id()
{
    return owner->sql(std::format(
        "SELECT s{}.id AS id FROM ({}) s{} ORDER BY id;", depth,
        cmd, depth));
}

inline GQL::Result GQL::Vertices::select(
    const std::string &_what)
{
    return owner->sql(std::format(
        "SELECT id, {} FROM ({}) ORDER BY id;", _what, cmd));
}

inline void GQL::Vertices::label(const std::string &_label)
{
    owner->sql(
        std::format("UPDATE nodes SET label = '{}' WHERE id IN "
                    "(SELECT s{}.id AS id FROM ({}) s{})",
                    _label, depth, cmd, depth));
}

inline void GQL::Vertices::tag(const std::string &_key,
                               const std::string &_value)
{
    owner->sql(std::format(
        "UPDATE nodes SET tags = json_set(tags, '$.{}', '{}') "
        "WHERE id IN (SELECT s{}.id AS id FROM ({}) s{})",
        _key, _value, depth, cmd, depth));
}

inline void GQL::Vertices::erase()
{
    owner->sql(
        std::format("DELETE FROM nodes WHERE id IN "
                    "(SELECT s{}.id AS id FROM ({}) s{})",
                    depth, cmd, depth));
}

inline GQL::Edges GQL::Vertices::in()
{
    return GQL::Edges(
        owner,
        std::format("SELECT * FROM edges WHERE target IN "
                    "(SELECT s{}.id AS id FROM ({}) s{})",
                    depth, cmd, depth),
        depth + 1);
}

inline GQL::Edges GQL::Vertices::out()
{
    return GQL::Edges(
        owner,
        std::format("SELECT * FROM edges WHERE source IN "
                    "(SELECT s{}.id AS id FROM ({}) s{})",
                    depth, cmd, depth),
        depth + 1);
}

inline void GQL::Vertices::add_edge(const GQL::Vertices &_other)
{
    owner->sql("INSERT INTO edges (source, target, tags) "
               "SELECT l.id, r.id, json('{}') " +
               std::format("FROM ({}) l CROSS JOIN ({}) r;",
                           cmd, _other.cmd));
}

////////////////////////////////////////////////////////////////

inline GQL::Edges GQL::Edges::where(
    const std::string &_sql_statement)
{
    return GQL::Edges(owner,
                      std::format("SELECT * FROM ({}) WHERE {}",
                                  cmd, _sql_statement),
                      depth + 1);
}

inline GQL::Edges GQL::Edges::with_label(
    const std::string &_label)
{
    return GQL::Edges(
        owner,
        std::format("SELECT * FROM ({}) s{} WHERE "
                    "s{}.label = '{}'",
                    cmd, depth, depth, _label),
        depth + 1);
}

inline GQL::Edges GQL::Edges::with_tag(
    const std::string &_key, const std::string &_value)
{
    return GQL::Edges(
        owner,
        std::format("SELECT * FROM ({}) s{} WHERE "
                    "json_extract(s{}.tags, '$.{}') = '{}'",
                    cmd, depth, depth, _key, _value),
        depth + 1);
}

inline GQL::Edges GQL::Edges::join(const GQL::Edges &_with)
{
    return GQL::Edges(
        owner,
        std::format("SELECT * FROM ({}) UNION ({})", cmd,
                    _with.cmd),
        depth + 1);
}

inline GQL::Edges GQL::Edges::intersection(
    const GQL::Edges &_with)
{
    return GQL::Edges(
        owner,
        std::format("SELECT * FROM ({}) INTERSECTION ({})", cmd,
                    _with.cmd),
        depth + 1);
}

inline GQL::Edges GQL::Edges::complement(
    const GQL::Edges &_universe)
{
    return GQL::Edges(
        owner,
        std::format("SELECT * FROM ({}) WHERE * NOT IN ({})",
                    _universe.cmd, cmd),
        depth + 1);
}

inline GQL::Result GQL::Edges::label()
{
    return owner->sql(
        std::format("SELECT s{}.id AS id, s{}.label FROM ({}) "
                    "s{} ORDER BY id;",
                    depth, depth, cmd, depth));
}

inline GQL::Result GQL::Edges::tag(const std::string &_key)
{
    return owner->sql(
        std::format("SELECT s{}.id AS id, "
                    "json_extract(s{}.tags, '$.{}') "
                    "AS {} FROM ({}) s{} ORDER BY id;",
                    depth, depth, _key, _key, cmd, depth));
}

inline GQL::Result GQL::Edges::id()
{
    return owner->sql(std::format(
        "SELECT s{}.id AS id FROM ({}) s{} ORDER BY id;", depth,
        cmd, depth));
}

inline GQL::Result GQL::Edges::select(const std::string &_what)
{
    return owner->sql(std::format(
        "SELECT id, {} FROM ({}) ORDER BY id;", _what, cmd));
}

inline void GQL::Edges::label(const std::string &_label)
{
    owner->sql(
        std::format("UPDATE edges SET label = '{}' WHERE id IN "
                    "(SELECT s{}.id AS id FROM ({}) s{})",
                    _label, depth, cmd, depth));
}

inline void GQL::Edges::tag(const std::string &_key,
                            const std::string &_value)
{
    owner->sql(std::format(
        "UPDATE edges SET tags = json_set(tags, '$.{}', '{}') "
        "WHERE id IN (SELECT s{}.id AS id FROM ({}) s{})",
        _key, _value, depth, cmd, depth));
}

inline void GQL::Edges::erase()
{
    owner->sql(std::format("DELETE FROM edges WHERE id IN "
                           "(SELECT s{}.id FROM ({}) s{})",
                           depth, cmd, depth));
}

inline GQL::Vertices GQL::Edges::source()
{
    return GQL::Vertices(
        owner,
        std::format("SELECT * FROM nodes WHERE id IN "
                    "(SELECT source AS s{}.id FROM ({}) s{})",
                    depth, cmd, depth),
        depth + 1);
}

inline GQL::Vertices GQL::Edges::target()
{
    return GQL::Vertices(
        owner,
        std::format("SELECT * FROM nodes WHERE id IN "
                    "(SELECT s{}.target AS id FROM ({}) s{})",
                    depth, cmd, depth),
        depth + 1);
}

////////////////////////////////////////////////////////////////

inline GQL::Vertices GQL::v()
{
    return GQL::Vertices(this, "SELECT * FROM nodes");
}

inline GQL::Edges GQL::e()
{
    return GQL::Edges(this, "SELECT * FROM edges");
}

inline GQL::Vertices GQL::v(const std::string &_where)
{
    return GQL::Vertices(this,
                         "SELECT * FROM nodes WHERE " + _where);
}

inline GQL::Edges GQL::e(const std::string &_where)
{
    return GQL::Edges(this,
                      "SELECT * FROM edges WHERE " + _where);
}

inline GQL::Vertices GQL::add_vertex()
{
    // Add vertex
    const auto id = next_node_id++;
    sql(std::format("INSERT INTO nodes (id, tags) VALUES ({}, ",
                    id) +
        "json('{}'));");

    // Return query
    return v(std::format("id = {}", id));
}

inline GQL::Vertices GQL::add_vertex(const uint64_t &_id)
{
    // Add vertex
    sql(std::format("INSERT INTO nodes (id, tags) VALUES ({}, ",
                    _id) +
        "json('{}'));");

    // Return query
    return v(std::format("id = {}", _id));
}

inline GQL::Edges GQL::add_edge(const uint64_t &_source,
                                const uint64_t &_target)
{
    // Add edge
    const auto id = next_edge_id++;
    sql(std::format(
            "INSERT INTO edges (id, source, target, tags) "
            "VALUES ({}, {}, {}, ",
            id, _source, _target) +
        "json('{}'));");

    // Return query
    return e(std::format("id = {}", id));
}

////////////////////////////////////////////////////////////////

inline GQL::Result GQL::sql(const std::string &_stmt)
{
    char *err_msg = nullptr;
    auto callback = [](void *out, int n_cols, char **col_vals,
                       char **col_names) {
        GQL::Result *q = (GQL::Result *)out;

        if (q->headers.empty())
        {
            for (int i = 0; i < n_cols; ++i)
            {
                if (col_names[i] == nullptr)
                {
                    q->headers.push_back("NULL");
                }
                else
                {
                    q->headers.push_back(col_names[i]);
                }
            }
        }

        std::vector<std::string> row;
        for (int i = 0; i < n_cols; ++i)
        {
            if (col_vals[i] == nullptr)
            {
                row.push_back("NULL");
            }
            else
            {
                row.push_back(col_vals[i]);
            }
        }
        q->body.push_back(row);

        return 0;
    };

    GQL::Result out;
    auto res = sqlite3_exec(db, _stmt.c_str(), callback, &out,
                            &err_msg);

    if (err_msg != NULL || res != SQLITE_OK)
    {
        std::cerr << "In SQL '" << _stmt << "':\n";
        throw std::runtime_error(err_msg);
    }

    return out;
}

inline void GQL::graphviz(const std::string &_filepath)
{
    // Open + header
    std::ofstream f(_filepath);
    if (!f.is_open())
    {
        throw std::runtime_error(std::format(
            "Failed to open output graphviz file '{}'.",
            _filepath));
    }
    f << "digraph {\n";

    // Nodes
    auto node_data = v().select("id, label");

    if (!node_data.body.empty())
    {
        auto id_i = node_data.index_of("id");
        auto label_i = node_data.index_of("label");

        for (const auto &node : node_data.body)
        {
            auto id = node[id_i];
            auto label = node[label_i];

            f << '\t' << id << " [label=\"" << label
              << "\"];\n";
        }
    }

    // Edges
    auto edge_data = e().select("source, target, label");

    if (!edge_data.body.empty())
    {
        auto source_i = edge_data.index_of("source");
        auto target_i = edge_data.index_of("target");
        auto label_i = edge_data.index_of("label");

        for (const auto &edge : edge_data.body)
        {
            auto source = edge[source_i];
            auto target = edge[target_i];
            auto label = edge[label_i];

            f << '\t' << source << " -> " << target
              << " [label=\"" << label << "\"];\n";
        }
    }

    // Footer + close
    f << "}\n";
    f.close();
}

inline std::ostream &operator<<(std::ostream &_strm,
                                const GQL::Result &_res)
{
    // Headers
    for (uint64_t i = 0; i < _res.headers.size(); ++i)
    {
        if (i != 0)
        {
            _strm << '|';
        }
        _strm << _res.headers[i];
    }
    _strm << '\n';

    // Body
    for (uint64_t row = 0; row < _res.body.size(); ++row)
    {
        for (uint64_t i = 0; i < _res.headers.size(); ++i)
        {
            if (i != 0)
            {
                _strm << '|';
            }
            _strm << _res.body[row][i];
        }
        _strm << '\n';
    }

    return _strm;
}

////////////////////////////////////////////////////////////////

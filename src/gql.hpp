/**
 * @file gql.hpp / libgqlite3.hpp
 *
 * @author J Dehmel
 *
 * @brief A manager object which maintains a SQLite3 instance.
 * Based on work partially funded by Colorado Mesa University.
 * All functions are inline here.
 *
 * NOTE: `__gql_format_str` is exposed upon inclusion: This
 * works like `std::format`, but is implemented much worse in
 * cases where \<format\> is not present at compile-time.
 * Defining `FORCE_CUSTOM_FORMAT` to be true-like will force the
 * worse implementation even if \<format\> is found.
 */

/*
This software is available via the MIT license, which is
transcribed below. Feel free to simply copy this header into
your project with no acknowledgement or citation needed.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MIT License

Copyright (c) 2024 Jordan Dehmel

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

// Assert compiler version
static_assert(__cplusplus > 201100L, "Invalid C++ std.");

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

/// The major (MAJ.xxx.xxx) version of GQL
const static uint GQL_MAJOR_VERSION = 000;

/// The minor (xxx.MIN.xxx) version of GQL
const static uint GQL_MINOR_VERSION = 002;

/// The patch (xxx.xxx.PAT) version of GQL
const static uint GQL_PATCH_VERSION = 003;

/**
 * @var GQL_VERSION
 * @brief The GQL version. This only ever increases with
 * versions. This can (and should) be used in static assertions
 * to ensure validity.
 */
const static uintmax_t GQL_VERSION =
    ((GQL_MAJOR_VERSION * 1000) + GQL_MINOR_VERSION) * 1000 +
    GQL_PATCH_VERSION;

// If std::format exists, use that. Otherwise, use custom (bad)
// formatting template.
#if (__has_include(<format>) && \
     __cplusplus >= 202000L && \
     !FORCE_CUSTOM_FORMAT)
#include <format>
#define __gql_format_str std::format
#else

namespace std {
/**
 * @brief A unit conversion needed for templating
 * @param _what A std::string to return
 * @returns The unchanged argument
 */
inline std::string to_string(const std::string &_what) {
  return _what;
}
} // namespace std

/**
 * @brief Bad custom version of std::format to be used in case
 * of emergency. Uses std::to_string for its inputs.
 * @tparam Types The type(s) of the `_args` parameter(s)
 * @param _format The format string to use, where "{}" will be
 * replaced.
 * @param _args Zero or more arguments to insert into
 * corresponding replacement spots in the format string.
 */
template <typename... Types>
std::string __gql_format_str(const std::string &_format,
                             Types... _args) {
  const std::vector<std::string> args = {
      std::to_string(_args)...};
  std::vector<std::string> parts;

  // Build here
  uint64_t str_i = 0, arg_i = 0;
  std::string partial;
  partial.reserve(_format.size());

  while (str_i < _format.size()) {
    if (_format[str_i] == '{') {
      if (!partial.empty()) {
        parts.push_back(partial);
        partial.clear();
      }

      if (str_i + 1 >= _format.size()) {
        throw std::runtime_error("Malformed format string.");
      }
      if (arg_i >= args.size()) {
        throw std::runtime_error("Malformed format string.");
      }

      ++str_i;

      if (_format[str_i] != '}') {
        throw std::runtime_error("Malformed format string.");
      }

      parts.push_back(args[arg_i]);
      ++arg_i;
    } else {
      partial.push_back(_format[str_i]);
    }

    ++str_i;
  }
  if (!partial.empty()) {
    parts.push_back(partial);
  }

  // Construct and return
  uint64_t size = 0;
  for (const auto &item : parts) {
    size += item.size();
  }

  std::string out;
  out.reserve(size);

  for (const auto &item : parts) {
    out += item;
  }
  return out;
}

#endif

/**
 * @var GQL_BOUNCE_THRESH
 * @brief After this many characters, the query "bounces". This
 * ensures that SQLite will not experience parser overflows.
 * "Bouncing" replaces the existing query with its ids, leading
 * to a less complicated (although not necessarily shorter)
 * query. This must remain preprocessor defined, since we may
 * want to change this via the command line at compile time.
 * Defaults to 128.
 */
#ifndef GQL_BOUNCE_THRESH
#define GQL_BOUNCE_THRESH 128
#endif

/**
 * @brief Encodes a string in hex to avoid SQLite JSON issues.
 * Anything encoded this way will not collide.
 * @param _what The string to encode
 * @returns The hexidecimal encoding of the given string
 */
inline std::string hex_encode(const std::string &_what) {
  const static char to_hex[] = "0123456789ABCDEF";
  std::string out;
  for (const char &c : _what) {
    out.push_back(to_hex[(c >> 4) & 0b1111]);
    out.push_back(to_hex[c & 0b1111]);
  }
  return out;
}

/**
 * @brief Decodes a string from hex to avoid SQLite JSON issues.
 * @param _what The hexidecimal string to decode
 * @returns The string such that hex_encode(returned) == _what
 */
inline std::string hex_decode(const std::string &_what) {
  std::string out;
  for (size_t i = 0; i < _what.size(); i += 2) {
    uint8_t first = ('0' <= _what[i] && _what[i] <= '9')
                        ? (_what[i] - '0')
                        : (10 + (_what[i] - 'A'));
    uint8_t second =
        ('0' <= _what[i + 1] && _what[i + 1] <= '9')
            ? (_what[i + 1] - '0')
            : (10 + (_what[i + 1] - 'A'));
    out.push_back((first << 4) | second);
  }
  return out;
}

/**
 * @class GQL
 * @brief GQL (Graph SQLite3) manager object. This is a database
 * instance which manages a property graph \f$ G = (V, E,
 * \lambda,* \mu) \f$, where \f$ V \f$ is the set of vertices,
 * \f$ E \f$ is the set of edges, \f$ \lambda: (E \cup V) \to
 * \Sigma \f$ associates a label from an arbitrary label set
 * \f$ \Sigma \f$ to each edge and vertex, and \f$ \mu: (E \cup
 * V) \times K \to S \f$ associates keys from arbitrary key
 * set \f$ K \f$ to values from arbitrary value set \f$ S \f$.
 * In our case, \f$ \Sigma, K \f$ and \f$ S \f$ are all
 * `std::string`.
 */
class GQL {
public:
  /**
   * @class GQL::Result
   * @brief The result of a completed GQL query
   */
  class Result {
  public:
    /// The rows of the returned table
    std::vector<std::vector<std::string>> body;

    /// The headers of the returned table
    std::vector<std::string> headers;

    /**
     * @brief Merge rows. Every row in this must have some
     * row in the other for which the ids match.
     * @param _other The result to merge with
     */
    inline void merge_rows(const Result &_other) {
      const auto other_indices = _other["id"];
      std::map<int, int> to_other;
      for (uint i = 0; i < size(); ++i) {
        to_other[i] = std::distance(
            other_indices.begin(),
            std::find(
                other_indices.begin(),
                other_indices.end(), operator[]("id")[i]));
      }

      // For each column in other which is not in this
      for (const auto &col : _other.headers) {
        if (std::find(headers.begin(), headers.end(), col) !=
            headers.end()) {
          continue;
        }

        // Append header
        const auto col_vals = _other[col];
        headers.push_back(col);
        for (uint i = 0; i < size(); ++i) {
          body[i].push_back(col_vals.at(to_other[i]));
        }
      }
    }

    /**
     * @brief Get row
     * @param _i The index of the row to get
     * @returns All the entries at that row
     */
    inline const std::vector<std::string> &
    operator[](const size_t &_i) const {
      return body[_i];
    }

    /**
     * @brief Get column
     * @param _col The column header value
     * @returns The values of each row at the given column
     */
    inline const std::vector<std::string>
    operator[](const std::string &_col) const {
      std::vector<std::string> out;
      out.reserve(size());
      if (empty()) {
        return out;
      }

      const auto ind = index_of(_col);

      for (uint i = 0; i < size(); ++i) {
        out.push_back(operator[](i)[ind]);
      }

      return out;
    }

    /**
     * @brief Begin const iteration
     * @returns A vector<vector<string>> const iterator
     */
    std::vector<std::vector<std::string>>::const_iterator
    begin() const {
      return body.begin();
    }

    /**
     * @brief End const iteration
     * @returns A vector<vector<string>> const ending
     * iterator
     */
    std::vector<std::vector<std::string>>::const_iterator
    end() const {
      return body.end();
    }

    // Get the number of items in this result
    /**
     * @brief Return the number of rows in the result
     * @returns The size of the underlying result body
     */
    inline size_t size() const noexcept {
      return body.size();
    }

    /**
     * @brief Returns true iff the result is empty
     * @returns The emptiness of the result
     */
    inline bool empty() const noexcept {
      return body.empty();
    }

    /**
     * @brief Returns the index of the given column (given
     * that it exists)
     * @returns The index such that headers.at(index) ==
     * _what
     */
    inline size_t index_of(const std::string &_what) const {
      auto it =
          std::find(headers.begin(), headers.end(), _what);
      if (it == headers.end()) {
        throw std::runtime_error(__gql_format_str(
            "Header value '{}' is not present in "
            "results.",
            _what));
      }
      return std::distance(headers.begin(), it);
    }
  };

  // Unit forward declaration
  class Edges;

  /**
   * @class GQL::Vertices
   * @brief A class representing zero or more vertices in the
   * graph
   */
  class Vertices {
  protected:
    /// A pointer to the owner of this instance: Vertices
    /// cannot be instantiated without one
    GQL *const owner;

    /// The SQL command which can be "collapsed" to resolve
    /// the query
    std::string cmd;

  public:
    /**
     * @brief Construct a Vertices object from some SQL
     * query. Bounces may happen herein.
     * @param _owner The GQL instance which is authoritative
     * over this object.
     * @param _cmd The command representing the query this
     * object has been constructed around
     */
    Vertices(GQL *const _owner, const std::string &_cmd);

    /**
     * @brief Copy from another instance FROM THE SAME OWNER
     * @param _other The object to copy from
     * @returns This object
     */
    Vertices &operator=(const Vertices &_other) {
      assert(owner == _other.owner);
      cmd = _other.cmd;
      return *this;
    }

    /**
     * @brief Copy from another instance FROM THE SAME OWNER
     * @param _other The object to copy from
     */
    inline constexpr Vertices(const Vertices &_other)
        : owner(_other.owner), cmd(_other.cmd) {
    }

    /**
     * @brief Select some sized subset
     * @param _n The size of the selected group
     * @returns A sized subset
     */
    Vertices limit(const uint64_t &_n) const;

    /**
     * @brief Gets the subset of items with the given label
     * @param _label The desired label
     * @returns All items contained herein which have the
     * given label
     */
    Vertices with_label(const std::string &_label) const;

    /**
     * @brief Select all vertices where the given tag key is
     * associated with the given value
     * @param _key The key to check the tags for
     * @param _value The value which must be associated
     * @returns Some subset of this where the given key
     * yields the given value
     */
    Vertices with_tag(const std::string &_key,
                      const std::string &_value) const;

    /// Select all vertices with the given id
    Vertices with_id(const uint64_t &_id) const;

    /// Select all vertices for which the given lambda
    /// returns true
    Vertices
    where(const std::function<bool(const Vertices &)> &_fn);

    /// Select all vertices which are in this, the passed
    /// set, or both
    Vertices join(const Vertices &_other) const;

    /// Select all vertices which are in both this and the
    /// passed set
    Vertices intersection(const Vertices &_other) const;

    /// Select all vertices which are in the universe set
    /// but not this set
    Vertices complement(const Vertices &_universe) const;

    /// Select all vertices which are in this set but not
    /// the given subgroup
    Vertices excluding(const Vertices &_subgroup) const;

    /// Select the label from all nodes in this set
    Result label() const;

    /// Select the value associated with the given key for
    /// all vertices in this set
    Result tag(const std::string &_key) const;

    /// Select the value associated with the given keys for
    /// all vertices in this set
    Result tag(const std::list<std::string> &_keys) const;

    /// Get the set of all keys
    std::list<std::string> keys() const;

    /// Select the id of all vertices in this set
    std::list<uint64_t> id() const;

    /// Set the label associated with all the nodes herein
    Vertices label(const std::string &_label);

    /**
     * @brief Set some key-value pair for all items herein
     * @param _key The key to associate
     * @param _value The value to associate
     * @returns This set after association
     */
    Vertices tag(const std::string &_key,
                 const std::string &_value);

    /// Erase these nodes from the database. This will also
    /// erase any and all edges which reference the deleted
    /// nodes as either source or target.
    void erase();

    /**
     * @brief Run some function given this set, then
     * continue on. For instance,
     * ```cpp
     * a.lemma([](auto self){
     *    self.tag("foo", "fizz"); }
     * ).erase();
     * ```
     * would execute a tag, then erase on the same node.
     * @param _fn The function to call with this set as an
     * argument
     * @returns This set after the function has been run
     */
    Vertices lemma(const std::function<void(Vertices)> _fn);

    /// Add edges from each node in this set to each node
    /// in the passed set (cartesian product).
    Edges add_edge(const Vertices &_to) const;

    /// Get all edges leading into these vertices
    Edges in() const;

    /// Get all edges leading out of these vertices
    Edges out() const;

    /// Gets only the vertices with the given in degree
    Vertices with_in_degree(const uint64_t &_count) const;

    /// Gets only the vertices with the given out degree
    Vertices with_out_degree(const uint64_t &_count) const;

    /// Gets the in-degrees of the given nodes
    Result
    in_degree(const std::string &_where_edge = "1") const;

    /// Gets the out-degrees of the given nodes
    Result
    out_degree(const std::string &_where_edge = "1") const;

    /**
     * @brief Break into a std::list where each entry is a
     * single vertex. This is useful for foreach operations.
     */
    std::list<Vertices> each() const;

    friend class Edges;
  };

  /**
   * @class GQL::Edges
   * @brief A class representing zero or more edges in a GQL
   * graph
   */
  class Edges {
  protected:
    /// A pointer to the owner of this instance: Edges
    /// cannot be instantiated without one
    GQL *const owner;

    /// The SQL command which can be "collapsed" to resolve
    /// the query
    std::string cmd;

  public:
    /**
     * @brief Construct an Edges object from some SQL
     * query. Bounces may happen herein.
     * @param _owner The GQL instance which is authoritative
     * over this object.
     * @param _cmd The command representing the query this
     * object has been constructed around
     */
    Edges(GQL *const _owner, const std::string &_cmd);

    /**
     * @brief Copy from another instance FROM THE SAME OWNER
     * @param _other The object to copy from
     * @returns This object
     */
    Edges &operator=(const Edges &_other) {
      assert(owner == _other.owner);
      cmd = _other.cmd;
      return *this;
    }

    /**
     * @brief Copy from another instance FROM THE SAME OWNER
     * @param _other The object to copy from
     */
    inline constexpr Edges(const Edges &_other)
        : owner(_other.owner), cmd(_other.cmd) {
    }

    /**
     * @brief Select some sized subset
     * @param _n The size of the selected group
     * @returns A sized subset
     */
    Edges limit(const uint64_t &_n) const;

    /// Select all edges with their source in the given set
    Edges with_source(const GQL::Vertices &_source) const;

    /// Select all edges with their target in the given set
    Edges with_target(const GQL::Vertices &_source) const;

    /**
     * @brief Gets the subset of items with the given label
     * @param _label The desired label
     * @returns All items contained herein which have the
     * given label
     */
    Edges with_label(const std::string &_label) const;

    /**
     * @brief Select all vertices where the given tag key is
     * associated with the given value
     * @param _key The key to check the tags for
     * @param _value The value which must be associated
     * @returns Some subset of this where the given key
     * yields the given value
     */
    Edges with_tag(const std::string &_key,
                   const std::string &_value) const;

    /// Get all edges which have the given id
    Edges with_id(const uint64_t &_id) const;

    /// Select all vertices for which the given lambda
    /// returns true
    Edges where(const std::function<bool(const Edges &)> &_fn);

    /// Return all values which are in either this set or
    /// the passed one or both
    Edges join(const Edges &_other) const;

    /// Return all values which are in both this set and the
    /// passed one
    Edges intersection(const Edges &_other) const;

    /// Return all values which are in the passed set but
    /// not in this set
    Edges complement(const Edges &_universe) const;

    /// Return all values which are in this set but not in
    /// the subgroup
    Edges excluding(const Edges &_subgroup) const;

    /// Get the label of all edges in this set
    Result label() const;

    /// Get the values associated with the given key
    Result tag(const std::string &_key) const;

    /// Get the values associated with the given keys
    Result tag(const std::list<std::string> &_keys) const;

    /// Get the set of all keys
    std::list<std::string> keys() const;

    /// Select the id of all edges in this set
    std::list<uint64_t> id() const;

    /// Set the label of these edges
    Edges label(const std::string &_label);

    /**
     * @brief Set some key-value pair for all items herein
     * @param _key The key to associate
     * @param _value The value to associate
     * @returns This set after association
     */
    Edges tag(const std::string &_key,
              const std::string &_value);

    /**
     * @brief Run some function given this set, then
     * continue on. For instance,
     * ```cpp
     * a.lemma([](auto self){
     *    self.tag("foo", "fizz"); }
     * ).erase();
     * ```
     * would execute a tag, then erase on the same node.
     * @param _fn The function to call with this set as an
     * argument
     * @returns This set after the function has been run
     */
    Edges lemma(const std::function<void(Edges)> _fn);

    /// Erase these edges from the database.
    void erase();

    /**
     * @brief Get any vertex in the graph from which one or
     * more of these edges point
     * @returns Some subset of the vertices
     */
    Vertices source() const;

    /**
     * @brief Get any vertex in the graph which is pointed
     * to by one or more of these edges
     * @returns Some subset of the vertices
     */
    Vertices target() const;

    /**
     * @brief Break into a std::list where each entry is a
     * single edge. This is useful for foreach operations.
     */
    std::list<Edges> each() const;
  };

  /**
   * @brief Initialize from a file, or create it if it DNE. If
   * _erase, purges all nodes and edges. If not _persistent,
   * purges after deletion.
   * @param _filepath The SQLite3 DB file to create/load
   * @param _erase True iff the DB should purge itself after
   * instantiation
   * @param _persistent False iff the DB should delete itself
   * upon destruction.
   */
  GQL(const std::string &_filepath = ":memory:",
      const bool &_erase = false,
      const bool &_persistent = true);

  /**
   * @brief Closes the database, possibly purging its file
   */
  ~GQL();

  /// Get the set of all vertices
  Vertices v();

  /// Get the set of all edges
  Edges e();

  /**
   * @brief Save a graphviz(.dot) representation of the
   * database
   * @param _filepath Where to save the graph representation
   */
  void graphviz(const std::filesystem::path &_filepath);

  /**
   * @brief Dump enough information about the graph to be
   * useful for debugging to the given output stream.
   * @param _into The stream to dump to
   */
  void dump(std::ostream &_into);

  /// Commit the database and open a new transaction
  void commit();

  /// Revert to the last commit and open a new transaction
  void rollback();

  /**
   * @brief Generate a new vertex and return a handle to it.
   * If this is the only method used, it will not cause
   * collisions.
   * @returns The newly-generated vertex
   */
  Vertices add_vertex();

  /**
   * @brief Generate a new vertex with the given id and return
   * a handle to it. The id given should be unique in the
   * set of vertices
   * @param _id The ID of the vertex to generate
   * @returns The newly-generated vertex
   */
  Vertices add_vertex(const uint64_t &_id);

  /**
   * @brief Generate a new edge from the given source to the
   * given target
   * @param _source The source of the edge
   * @param _target The target of the edge
   * @returns The newly-added edge
   */
  Edges add_edge(const uint64_t &_source,
                 const uint64_t &_target);

  /// Increments with each SQL query submitted (bounce or
  /// normal resolution)
  uint64_t sql_call_counter = 0;

  /// Getter for the filepath
  std::filesystem::path get_filepath() const noexcept;

protected:
  /// Get all vertices where some SQL WHERE clause holds
  Vertices v(const std::string &_where);

  /// Get all edges where some SQL WHERE clause holds
  Edges e(const std::string &_where);

  /// Execute the given sql and return the results
  Result sql(const std::string &_sql);

  /// A pointer to the underlying SQLite3 instance
  sqlite3 *db = nullptr;

  /// The next automatically-assigned ID values
  uint64_t next_node_id = 1;

  /// The next automatically-assigned ID values
  uint64_t next_edge_id = 1;

  /// Whether the DB should exist after destruction
  bool persistent;

  /// The database file managed by this object
  std::filesystem::path filepath;
};

/**
 * @brief Prints a given GQL::Result object
 * @param _into The output stream to insert into
 * @param _what The result object to print
 * @returns The stream after insertion
 */
std::ostream &operator<<(std::ostream &_into,
                         const GQL::Result &_what);

////////////////////////////////////////////////////////////////

inline GQL::GQL(const std::string &_filepath,
                const bool &_erase, const bool &_persistent)
    : persistent(_persistent), filepath(_filepath) {
  if (_erase && std::filesystem::exists(filepath)) {
    std::filesystem::remove_all(filepath);
  }

  // Open database
  sqlite3_open(filepath.c_str(), &db);

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

  // Indices here
  sql("CREATE INDEX IF NOT EXISTS edge_src "
      "ON edges(source);");
  sql("CREATE INDEX IF NOT EXISTS edge_tgt "
      "ON edges(target);");
  sql("CREATE INDEX IF NOT EXISTS edge_lbl "
      "ON edges(label);");
  sql("CREATE INDEX IF NOT EXISTS edge_id "
      "ON edges(label);");
  sql("CREATE INDEX IF NOT EXISTS node_label "
      "ON nodes(label);");
  sql("CREATE INDEX IF NOT EXISTS node_id "
      "ON nodes(id);");

  sql("BEGIN;");
}

inline GQL::~GQL() {
  sql("COMMIT;");
  sqlite3_close(db);

  if (!persistent && std::filesystem::exists(filepath)) {
    std::filesystem::remove_all(filepath);
  }
}

inline void GQL::commit() {
  sql("COMMIT;");
  sql("BEGIN;");
}

inline void GQL::rollback() {
  sql("ROLLBACK;");
  sql("BEGIN;");
}

////////////////////////////////////////////////////////////////

inline GQL::Vertices::Vertices(GQL *const _owner,
                               const std::string &_cmd)
    : owner(_owner), cmd(_cmd) {
  if (_cmd.size() > GQL_BOUNCE_THRESH) {
    // Perform query simplification "bounce"
    auto ids = this->id();

    std::string bounced_cmd =
        "SELECT * FROM nodes WHERE id IN (";
    if (ids.empty()) {
      bounced_cmd += ')';
    }
    for (const auto &id : ids) {
      bounced_cmd += std::to_string(id) + ",";
    }
    bounced_cmd.back() = ')';

    cmd = bounced_cmd;
  }
}

inline GQL::Vertices
GQL::Vertices::limit(const uint64_t &_n) const {
  return GQL::Vertices(
      owner,
      __gql_format_str("SELECT * FROM ({}) LIMIT {}", cmd, _n));
}

inline GQL::Vertices
GQL::Vertices::with_label(const std::string &_label) const {
  return GQL::Vertices(
      owner, __gql_format_str("SELECT * FROM ({}) WHERE "
                              "label = '{}'",
                              cmd, _label));
}

inline GQL::Vertices
GQL::Vertices::with_tag(const std::string &_key,
                        const std::string &_value) const {
  return GQL::Vertices(
      owner,
      __gql_format_str("SELECT * FROM ({}) WHERE "
                       "json_extract(tags, '$.{}') = '{}'",
                       cmd, hex_encode(_key), _value));
}

inline GQL::Vertices
GQL::Vertices::with_id(const uint64_t &_id) const {
  return GQL::Vertices(
      owner, __gql_format_str("SELECT * FROM ({}) WHERE "
                              "id = {}",
                              cmd, _id));
}

inline GQL::Vertices GQL::Vertices::where(
    const std::function<bool(const GQL::Vertices &)> &_fn) {
  // Set to empty set
  auto out = excluding(*this);

  for (const auto &item : each()) {
    if (_fn(item)) {
      // Merge in matches
      out = out.join(item);
    }
  }

  return out;
}

inline GQL::Vertices
GQL::Vertices::join(const GQL::Vertices &_with) const {
  return GQL::Vertices(
      owner, __gql_format_str("{} UNION {}", cmd, _with.cmd));
}

inline GQL::Vertices
GQL::Vertices::intersection(const GQL::Vertices &_with) const {
  return GQL::Vertices(
      owner,
      __gql_format_str("{} INTERSECT {}", cmd, _with.cmd));
}

inline GQL::Vertices GQL::Vertices::complement(
    const GQL::Vertices &_universe) const {
  return GQL::Vertices(
      owner,
      __gql_format_str("SELECT * FROM ({}) WHERE id NOT IN "
                       "(SELECT id FROM ({}))",
                       _universe.cmd, cmd));
}

inline GQL::Vertices
GQL::Vertices::excluding(const GQL::Vertices &_subgroup) const {
  return GQL::Vertices(
      owner,
      __gql_format_str("SELECT * FROM ({}) WHERE id NOT IN "
                       "(SELECT id FROM ({}))",
                       cmd, _subgroup.cmd));
}

inline GQL::Result GQL::Vertices::label() const {
  return owner->sql(
      __gql_format_str("SELECT id, label AS label FROM "
                       "({}) ORDER BY id;",
                       cmd));
}

inline GQL::Result
GQL::Vertices::tag(const std::string &_key) const {
  return owner->sql(
      __gql_format_str("SELECT id, "
                       "json_extract(tags, '$.{}') AS '{}' "
                       "FROM ({}) ORDER BY id;",
                       hex_encode(_key), _key, cmd));
}

inline GQL::Result
GQL::Vertices::tag(const std::list<std::string> &_keys) const {
  if (_keys.empty()) {
    return GQL::Result{};
  }

  std::string subcommand = "";
  for (const auto &key : _keys) {
    if (!subcommand.empty()) {
      subcommand += ", ";
    }

    if (key == "id" || key == "label") {
      subcommand += key;
    } else {
      subcommand +=
          __gql_format_str("json_extract(tags, '$.{}') AS '{}'",
                           hex_encode(key), key);
    }
  }

  return owner->sql(__gql_format_str(
      "SELECT {} FROM ({}) ORDER BY id;", subcommand, cmd));
}

inline std::list<std::string> GQL::Vertices::keys() const {
  auto res = owner->sql(
      __gql_format_str("SELECT DISTINCT key FROM ({}) JOIN "
                       "JSON_EACH(tags);",
                       cmd));
  std::list<std::string> out;

  for (uint i = 0; i < res.size(); ++i) {
    out.push_back(hex_decode(res.body[i][0]));
  }
  return out;
}

inline std::list<uint64_t> GQL::Vertices::id() const {
  std::list<uint64_t> out;
  auto res = owner->sql(__gql_format_str(
      "SELECT id FROM ({}) ORDER BY id;", cmd));
  for (const auto &id : res["id"]) {
    out.push_back(std::stoull(id));
  }
  return out;
}

inline GQL::Vertices
GQL::Vertices::label(const std::string &_label) {
  owner->sql(__gql_format_str(
      "UPDATE nodes SET label = '{}' WHERE id IN "
      "(SELECT id FROM ({}))",
      _label, cmd));

  return *this;
}

inline GQL::Vertices
GQL::Vertices::tag(const std::string &_key,
                   const std::string &_value) {
  owner->sql(__gql_format_str(
      "UPDATE nodes SET tags = json_set(tags, '$.{}', '{}') "
      "WHERE id IN (SELECT id FROM ({}))",
      hex_encode(_key), _value, cmd));

  return *this;
}

inline GQL::Vertices GQL::Vertices::lemma(
    const std::function<void(GQL::Vertices)> _fn) {
  _fn(*this);
  return *this;
}

inline void GQL::Vertices::erase() {
  owner->sql(__gql_format_str("DELETE FROM nodes WHERE id IN "
                              "(SELECT id FROM ({}));",
                              cmd));

  // Erase dead edges
  owner->sql("WITH ids AS (SELECT id FROM nodes) "
             "DELETE FROM edges WHERE "
             "source NOT IN ids "
             "OR target NOT IN ids;");
}

inline GQL::Edges GQL::Vertices::in() const {
  return GQL::Edges(
      owner,
      __gql_format_str("SELECT * FROM edges WHERE target IN "
                       "(SELECT id FROM ({}))",
                       cmd));
}

inline GQL::Edges GQL::Vertices::out() const {
  return GQL::Edges(
      owner,
      __gql_format_str("SELECT * FROM edges WHERE source IN "
                       "(SELECT id FROM ({}))",
                       cmd));
}

inline GQL::Vertices
GQL::Vertices::with_in_degree(const uint64_t &_count) const {
  return GQL::Vertices(
      owner, __gql_format_str(
                 "WITH n AS ({}) "
                 "SELECT id, label, tags FROM ("
                 "SELECT n.*, COUNT(e.id) AS c "
                 "FROM n LEFT JOIN (SELECT * FROM edges) e "
                 "ON e.target = n.id "
                 "GROUP BY n.id) t "
                 "WHERE t.c = {}",
                 cmd, _count));
}

inline GQL::Vertices
GQL::Vertices::with_out_degree(const uint64_t &_count) const {
  return GQL::Vertices(
      owner, __gql_format_str(
                 "WITH n AS ({}) "
                 "SELECT id, label, tags FROM ("
                 "SELECT n.*, COUNT(e.id) AS c "
                 "FROM n LEFT JOIN (SELECT * FROM edges) e "
                 "ON e.source = n.id "
                 "GROUP BY n.id) t "
                 "WHERE t.c = {}",
                 cmd, _count));
}

inline GQL::Result
GQL::Vertices::in_degree(const std::string &_where_edge) const {
  return owner->sql(__gql_format_str(
      "WITH n AS ({}) "
      "SELECT t.id AS id, t.c AS in_degree FROM ("
      "SELECT n.id AS id, COUNT(e.id) AS c "
      "FROM n LEFT JOIN (SELECT * FROM edges WHERE {}) e "
      "ON e.target = n.id "
      "GROUP BY n.id) t "
      "ORDER BY id;",
      cmd, _where_edge));
}

inline GQL::Result GQL::Vertices::out_degree(
    const std::string &_where_edge) const {
  return owner->sql(__gql_format_str(
      "WITH n AS ({}) "
      "SELECT t.id AS id, t.c AS out_degree FROM ("
      "SELECT n.id AS id, COUNT(e.id) AS c "
      "FROM n LEFT JOIN (SELECT * FROM edges WHERE {}) e "
      "ON e.source = n.id "
      "GROUP BY n.id) t "
      "ORDER BY id;",
      cmd, _where_edge));
}

inline std::list<GQL::Vertices> GQL::Vertices::each() const {
  std::list<GQL::Vertices> out;
  for (const auto &id : id()) {
    out.push_back(owner->v().with_id(id));
  }
  return out;
}

inline GQL::Edges
GQL::Vertices::add_edge(const GQL::Vertices &_other) const {
  owner->sql("INSERT INTO edges (source, target) "
             "SELECT l.id, r.id " +
             __gql_format_str("FROM ({}) l CROSS JOIN ({}) r",
                              cmd, _other.cmd));

  return GQL::Edges(
      owner, __gql_format_str("SELECT * FROM edges "
                              "WHERE (source, target) IN "
                              "(SELECT l.id, r.id FROM "
                              "({}) l CROSS JOIN ({}) r)",
                              cmd, _other.cmd));
}

////////////////////////////////////////////////////////////////

inline GQL::Edges::Edges(GQL *const _owner,
                         const std::string &_cmd)
    : owner(_owner), cmd(_cmd) {
  if (_cmd.size() > GQL_BOUNCE_THRESH) {
    // Perform query simplification "bounce"
    auto ids = this->id();

    std::string bounced_cmd =
        "SELECT * FROM edges WHERE id IN (";
    if (ids.empty()) {
      bounced_cmd += ')';
    }
    for (const auto &id : ids) {
      bounced_cmd += std::to_string(id) + ",";
    }
    bounced_cmd.back() = ')';

    cmd = bounced_cmd;
  }
}

inline GQL::Edges GQL::Edges::limit(const uint64_t &_n) const {
  return GQL::Edges(
      owner,
      __gql_format_str("SELECT * FROM ({}) LIMIT {}", cmd, _n));
}

inline GQL::Edges
GQL::Edges::with_source(const GQL::Vertices &_source) const {
  return GQL::Edges(
      owner, __gql_format_str("SELECT * FROM ({}) WHERE "
                              "source IN (SELECT id FROM ({}))",
                              cmd, _source.cmd));
}

inline GQL::Edges
GQL::Edges::with_target(const GQL::Vertices &_source) const {
  return GQL::Edges(
      owner, __gql_format_str("SELECT * FROM ({}) WHERE "
                              "target IN (SELECT id FROM ({}))",
                              cmd, _source.cmd));
}

inline GQL::Edges
GQL::Edges::with_label(const std::string &_label) const {
  return GQL::Edges(owner,
                    __gql_format_str("SELECT * FROM ({}) WHERE "
                                     "label = '{}'",
                                     cmd, _label));
}

inline GQL::Edges
GQL::Edges::with_tag(const std::string &_key,
                     const std::string &_value) const {
  return GQL::Edges(
      owner,
      __gql_format_str("SELECT * FROM ({}) WHERE "
                       "json_extract(tags, '$.{}') = '{}'",
                       cmd, hex_encode(_key), _value));
}

inline GQL::Edges
GQL::Edges::with_id(const uint64_t &_id) const {
  return GQL::Edges(owner,
                    __gql_format_str("SELECT * FROM ({}) WHERE "
                                     "id = {}",
                                     cmd, _id));
}

inline GQL::Edges GQL::Edges::where(
    const std::function<bool(const GQL::Edges &)> &_fn) {
  // Set to empty set
  auto out = excluding(*this);

  for (const auto &item : each()) {
    if (_fn(item)) {
      // Merge in matches
      out = out.join(item);
    }
  }

  return out;
}

inline GQL::Edges
GQL::Edges::join(const GQL::Edges &_with) const {
  return GQL::Edges(
      owner, __gql_format_str("{} UNION {}", cmd, _with.cmd));
}

inline GQL::Edges
GQL::Edges::intersection(const GQL::Edges &_with) const {
  return GQL::Edges(owner, __gql_format_str("{} INTERSECT {}",
                                            cmd, _with.cmd));
}

inline GQL::Edges
GQL::Edges::complement(const GQL::Edges &_universe) const {
  return GQL::Edges(
      owner,
      __gql_format_str("SELECT * FROM ({}) WHERE id NOT IN "
                       "(SELECT id FROM ({}))",
                       _universe.cmd, cmd));
}

inline GQL::Edges
GQL::Edges::excluding(const GQL::Edges &_subgroup) const {
  return GQL::Edges(
      owner,
      __gql_format_str("SELECT * FROM ({}) WHERE id NOT IN "
                       "(SELECT id FROM ({}))",
                       cmd, _subgroup.cmd));
}

inline GQL::Result GQL::Edges::label() const {
  return owner->sql(__gql_format_str(
      "SELECT id, label FROM ({}) ORDER BY id;", cmd));
}

inline GQL::Result
GQL::Edges::tag(const std::string &_key) const {
  return owner->sql(
      __gql_format_str("SELECT id, "
                       "json_extract(tags, '$.{}') "
                       "AS '{}' FROM ({}) ORDER BY id;",
                       hex_encode(_key), _key, cmd));
}

inline GQL::Result
GQL::Edges::tag(const std::list<std::string> &_keys) const {
  if (_keys.empty()) {
    return GQL::Result{};
  }

  std::string subcommand = "";
  for (const auto &key : _keys) {
    if (!subcommand.empty()) {
      subcommand += ", ";
    }

    if (key == "id" || key == "label" || key == "source" ||
        key == "target") {
      subcommand += key;
    } else {
      subcommand +=
          __gql_format_str("json_extract(tags, '$.{}') AS '{}'",
                           hex_encode(key), key);
    }
  }

  return owner->sql(__gql_format_str(
      "SELECT {} FROM ({}) ORDER BY id;", subcommand, cmd));
}

inline std::list<std::string> GQL::Edges::keys() const {
  auto res = owner->sql(
      __gql_format_str("SELECT DISTINCT key FROM ({}) JOIN "
                       "JSON_EACH(tags);",
                       cmd));
  std::list<std::string> out;
  for (uint i = 0; i < res.size(); ++i) {
    out.push_back(hex_decode(res.body[i][0]));
  }
  return out;
}

inline std::list<uint64_t> GQL::Edges::id() const {
  std::list<uint64_t> out;
  auto res = owner->sql(__gql_format_str(
      "SELECT id FROM ({}) ORDER BY id;", cmd));
  for (const auto &id : res["id"]) {
    out.push_back(std::stoull(id));
  }
  return out;
}

inline GQL::Edges GQL::Edges::label(const std::string &_label) {
  owner->sql(__gql_format_str(
      "UPDATE edges SET label = '{}' WHERE id IN "
      "(SELECT id FROM ({}))",
      _label, cmd));

  return *this;
}

inline GQL::Edges GQL::Edges::tag(const std::string &_key,
                                  const std::string &_value) {
  owner->sql(__gql_format_str(
      "UPDATE edges SET tags = json_set(tags, '$.{}', '{}') "
      "WHERE id IN (SELECT id FROM ({}))",
      hex_encode(_key), _value, cmd));

  return *this;
}

inline GQL::Edges
GQL::Edges::lemma(const std::function<void(GQL::Edges)> _fn) {
  _fn(*this);
  return *this;
}

inline void GQL::Edges::erase() {
  owner->sql(__gql_format_str("DELETE FROM edges WHERE id IN "
                              "(SELECT id FROM ({}))",
                              cmd));
}

inline GQL::Vertices GQL::Edges::source() const {
  return GQL::Vertices(
      owner, __gql_format_str("SELECT * FROM nodes WHERE id IN "
                              "(SELECT source AS id FROM ({}))",
                              cmd));
}

inline GQL::Vertices GQL::Edges::target() const {
  return GQL::Vertices(
      owner, __gql_format_str("SELECT * FROM nodes WHERE id IN "
                              "(SELECT target AS id FROM ({}))",
                              cmd));
}

inline std::list<GQL::Edges> GQL::Edges::each() const {
  std::list<GQL::Edges> out;
  for (const auto &id : id()) {
    out.push_back(owner->e().with_id(id));
  }
  return out;
}

////////////////////////////////////////////////////////////////

inline GQL::Vertices GQL::v() {
  return GQL::Vertices(this, "SELECT * FROM nodes");
}

inline GQL::Edges GQL::e() {
  return GQL::Edges(this, "SELECT * FROM edges");
}

inline GQL::Vertices GQL::v(const std::string &_where) {
  return GQL::Vertices(this,
                       "SELECT * FROM nodes WHERE " + _where);
}

inline GQL::Edges GQL::e(const std::string &_where) {
  return GQL::Edges(this,
                    "SELECT * FROM edges WHERE " + _where);
}

inline GQL::Vertices GQL::add_vertex() {
  // Add vertex
  const auto id = next_node_id++;
  sql(__gql_format_str(
          "INSERT INTO nodes (id, tags) VALUES ({}, ", id) +
      "json('{}'));");

  // Return query
  return v(__gql_format_str("id = {}", id));
}

inline GQL::Vertices GQL::add_vertex(const uint64_t &_id) {
  // Add vertex
  sql(__gql_format_str(
          "INSERT INTO nodes (id, tags) VALUES ({}, ", _id) +
      "json('{}'));");

  // Return query
  return v(__gql_format_str("id = {}", _id));
}

inline GQL::Edges GQL::add_edge(const uint64_t &_source,
                                const uint64_t &_target) {
  // Add edge
  const auto id = next_edge_id++;
  sql(__gql_format_str(
          "INSERT INTO edges (id, source, target, tags) "
          "VALUES ({}, {}, {}, ",
          id, _source, _target) +
      "json('{}'));");

  // Return query
  return e(__gql_format_str("id = {}", id));
}

////////////////////////////////////////////////////////////////

inline GQL::Result GQL::sql(const std::string &_stmt) {
  ++sql_call_counter;

  char *err_msg = nullptr;
  const static auto callback = [](void *out, int n_cols,
                                  char **col_vals,
                                  char **col_names) {
    GQL::Result *q = (GQL::Result *)out;

    if (q->headers.empty()) {
      for (int i = 0; i < n_cols; ++i) {
        if (col_names[i] == nullptr) {
          q->headers.push_back("NULL");
        } else {
          q->headers.push_back(col_names[i]);
        }
      }
    }

    std::vector<std::string> row;
    for (int i = 0; i < n_cols; ++i) {
      if (col_vals[i] == nullptr) {
        row.push_back("NULL");
      } else {
        row.push_back(col_vals[i]);
      }
    }
    q->body.push_back(row);

    return 0;
  };

  GQL::Result out;
  auto res =
      sqlite3_exec(db, _stmt.c_str(), callback, &out, &err_msg);

  if (err_msg != NULL || res != SQLITE_OK) {
    std::cerr << "In SQL '" << _stmt << "':\n";
    throw std::runtime_error(err_msg);
  }

  return out;
}

inline void GQL::dump(std::ostream &_into) {
  _into << sql("SELECT * FROM nodes;") << "\n\n"
        << sql("SELECT * FROM edges;") << "\n";
}

inline void
GQL::graphviz(const std::filesystem::path &_filepath) {
  const static auto sanitize = [](std::string &_w) -> void {
    for (uint64_t i = 0; i < _w.size(); ++i) {
      switch (_w[i]) {
      case '"':
        _w.replace(i, 1, "\\\"");
      case '\\':
        ++i;
      }
    }
  };

  // Open + header
  std::ofstream f(_filepath);
  if (!f.is_open()) {
    throw std::runtime_error(__gql_format_str(
        "Failed to open output graphviz file '{}'.",
        _filepath.string()));
  }
  f << "digraph {\n\tforcelabels=true;\n";

  auto node_data =
      v().tag(std::list<std::string>{"id", "label"});
  auto nodes_tags = v().tag(v().keys());
  auto edge_data = e().tag(
      std::list<std::string>{"source", "target", "label"});
  auto edge_tags = e().tag(e().keys());

  // Nodes
  if (!node_data.body.empty()) {
    auto id_i = node_data.index_of("id");
    auto label_i = node_data.index_of("label");

    uint index = 0;
    for (const auto &node : node_data.body) {
      auto id = node[id_i];
      auto label = node[label_i];
      std::string tags;
      for (const auto &key : nodes_tags.headers) {
        if (!tags.empty()) {
          tags += ",";
        }
        tags += "\"" + hex_decode(key) + "\": \"" +
                nodes_tags[key].at(index) + "\"";
      }
      tags = "{" + tags + "}";

      sanitize(label);
      sanitize(tags);

      f << '\t' << id << " [label=\"" << label
        << "\", xlabel=\"" << tags << "\"];\n";

      ++index;
    }
  }

  // Edges
  if (!edge_data.body.empty()) {
    auto source_i = edge_data.index_of("source");
    auto target_i = edge_data.index_of("target");
    auto label_i = edge_data.index_of("label");

    for (const auto &edge : edge_data.body) {
      auto source = edge[source_i];
      auto target = edge[target_i];
      auto label = edge[label_i];
      std::string tags;
      for (const auto &item : edge_tags) {
        if (!tags.empty()) {
          tags += ",";
        }
        tags += std::string("\"") + hex_decode(item.at(0)) +
                std::string("\": \"") + item.at(1) +
                std::string("\"");
      }
      tags = "{" + tags + "}";

      sanitize(label);
      sanitize(tags);

      f << '\t' << source << " -> " << target << " [label=\""
        << label << "\", xlabel=\"" << tags << "\"];\n";
    }
  }

  // Footer + close
  f << "}\n";
  f.close();
}

inline std::ostream &operator<<(std::ostream &_strm,
                                const GQL::Result &_res) {
  // Headers
  for (uint64_t i = 0; i < _res.headers.size(); ++i) {
    if (i != 0) {
      _strm << '|';
    }
    _strm << _res.headers[i];
  }
  _strm << '\n';

  // Body
  for (uint64_t row = 0; row < _res.body.size(); ++row) {
    for (uint64_t i = 0; i < _res.headers.size(); ++i) {
      if (i != 0) {
        _strm << '|';
      }
      _strm << _res.body[row][i];
    }
    _strm << '\n';
  }

  return _strm;
}

/// Getter for the filepath
inline std::filesystem::path
GQL::get_filepath() const noexcept {
  return filepath;
}

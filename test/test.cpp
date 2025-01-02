/*
Unit tests for GQL
*/

#include "../src/gql.hpp"

#include <cassert>
#include <filesystem>
#include <set>
#include <string>
#include <sys/types.h>

// If the two vectors do not match, cause a kernel panic
inline void assert_eq(const std::vector<std::string> &_l,
                      const std::vector<std::string> &_r)
{
    bool flag = false;

    if (_l.size() == _r.size())
    {
        flag = true;
        for (uint64_t i = 0; i < _l.size(); ++i)
        {
            if (_l[i] != _r[i])
            {
                flag = false;
                break;
            }
        }
    }

    if (!flag)
    {
        std::cout << "Failed assert_eq w/ input:\n";
        for (const auto &item : _l)
        {
            std::cout << item << ' ';
        }
        std::cout << '\n';
        for (const auto &item : _r)
        {
            std::cout << item << ' ';
        }
        std::cout << '\n';
    }

    assert(flag);
}

void test_merge_rows()
{
    GQL::Result a, b;
    a.headers = {"id", "fizz"};
    a.body = {
        {"1", "fizz1"}, {"23", "fizz23"}, {"98", "fizz98"}};

    b.headers = {"buzz", "id"};
    b.body = {{"buzz98", "98"},
              {"buzz1", "1"},
              {"buzz40", "40"},
              {"buzz23", "23"}};

    a.merge_rows(b);

    assert_eq(a.headers, {"id", "fizz", "buzz"});
    assert(a.size() == 3);
    assert_eq(a.body[0], {"1", "fizz1", "buzz1"});
    assert_eq(a.body[1], {"23", "fizz23", "buzz23"});
    assert_eq(a.body[2], {"98", "fizz98", "buzz98"});

    bool flag = false;
    try
    {
        b.merge_rows(a);
    }
    catch (...)
    {
        flag = true;
    }
    assert(flag);
}

void test_limit()
{
    GQL g("foo.db", true);

    g.add_vertex(3);
    g.add_vertex(2);
    g.add_vertex(1);

    auto r = g.v().limit(1).id();
    assert(r.size() == 1);
    assert(r["id"][0] == "1");
}

void test_set_operations()
{
    // Tests intersection, join, complement, and exclusion.
    GQL g("foo.db", true);

    for (int i = 1; i <= 5; ++i)
    {
        g.add_vertex(i);
        g.add_edge(i, i);
    }

    // Basic sets
    auto a = g.v().where("id <= 3");
    auto b = g.v().where("id >= 3");
    auto universe = g.v();

    auto a_edges = a.out();
    auto b_edges = b.out();
    auto universe_edges = g.e();

    // Vertices
    {
        // Experimental sets
        auto u = a.join(b);
        auto i = a.intersection(b);
        auto a_minus_b = a.excluding(b);
        auto b_minus_a = b.excluding(a);
        auto a_complement = a.complement(universe);
        auto b_complement = b.complement(universe);

        assert(u.id().size() == 5);
        assert(i.id().size() == 1);
        assert(i.id()["id"][0] == "3");
        assert(a_minus_b.id().size() == 2);
        assert(a_minus_b.id()["id"][0] == "1");
        assert(a_minus_b.id()["id"][1] == "2");
        assert(b_minus_a.id().size() == 2);
        assert(b_minus_a.id()["id"][0] == "4");
        assert(b_minus_a.id()["id"][1] == "5");
        assert(a_complement.id().size() == 2);
        assert(a_complement.id()["id"][0] == "4");
        assert(a_complement.id()["id"][1] == "5");
        assert(b_complement.id().size() == 2);
        assert(b_complement.id()["id"][0] == "1");
        assert(b_complement.id()["id"][1] == "2");
    }

    // Edges
    {
        // Experimental sets
        auto u = a_edges.join(b_edges).target();
        auto i = a_edges.intersection(b_edges).target();
        auto a_minus_b = a_edges.excluding(b_edges).target();
        auto b_minus_a = b_edges.excluding(a_edges).target();
        auto a_complement =
            a_edges.complement(universe_edges).target();
        auto b_complement =
            b_edges.complement(universe_edges).target();

        assert(u.id().size() == 5);
        assert(i.id().size() == 1);
        assert(i.id()["id"][0] == "3");
        assert(a_minus_b.id().size() == 2);
        assert(a_minus_b.id()["id"][0] == "1");
        assert(a_minus_b.id()["id"][1] == "2");
        assert(b_minus_a.id().size() == 2);
        assert(b_minus_a.id()["id"][0] == "4");
        assert(b_minus_a.id()["id"][1] == "5");
        assert(a_complement.id().size() == 2);
        assert(a_complement.id()["id"][0] == "4");
        assert(a_complement.id()["id"][1] == "5");
        assert(b_complement.id().size() == 2);
        assert(b_complement.id()["id"][0] == "1");
        assert(b_complement.id()["id"][1] == "2");
    }
}

void test_lemma()
{
    GQL g("foo.db", true);
    double sum, prod, sum_squared;

    // Setup
    for (int i = 1; i <= 5; ++i)
    {
        g.add_vertex(i).label(std::to_string(i));
    }

    for (int i = 1; i <= 5; ++i)
    {
        for (int j = 1; j <= 5; ++j)
        {
            g.v()
                .with_id(i)
                .add_edge(g.v().with_id(j))
                .label(std::to_string(i * j));
        }
    }

    // Testing
    g.v()
        .lemma([&](auto v) {
            sum = 0.0;
            for (const auto &l : v.label()["label"])
            {
                sum += std::stod(l);
            }
        })
        .lemma([&](auto v) {
            prod = 1.0;
            for (const auto &l : v.label()["label"])
            {
                prod *= std::stod(l);
            }
        })
        .lemma([&](auto v) {
            sum_squared = 0.0;
            for (const auto &l : v.label()["label"])
            {
                double val = std::stod(l);
                sum_squared += val * val;
            }
        });

    assert(sum == (1 + 2 + 3 + 4 + 5));
    assert(sum_squared == (1 + 4 + 9 + 16 + 25));
    assert(prod == (1 * 2 * 3 * 4 * 5));

    g.e().lemma([&](auto e) {
        sum = 0.0;
        for (const auto &l : e.label()["label"])
        {
            sum += std::stod(l);
        }
    });

    assert(sum == (1 + 2 + 3 + 4 + 5 + 2 + 4 + 6 + 8 + 10 + 3 +
                   6 + 9 + 12 + 15 + 4 + 8 + 12 + 16 + 20 + 5 +
                   10 + 15 + 20 + 25));
}

void test_multiple_tag_getter()
{
    GQL g("foo.db", true);
    g.add_vertex(1).tag("a", "1").tag("b", "2").tag("c", "3");

    auto r = g.v().with_id(1).tag({"a", "b", "c"});

    assert(r.size() == 1);
    assert(r["a"][0] == "1");
    assert(r["b"][0] == "2");
    assert(r["c"][0] == "3");
}

void test_open()
{
    // Initial write
    {
        GQL g1("foo.db", true);
        g1.add_vertex(123).label("this should be gone");
        g1.add_edge(123, 123).label("nor should this");
        g1.commit();
    }

    // Open without erasure
    {
        GQL g2("foo.db");
        assert(g2.v().id().size() == 1);
        assert(g2.e().id().size() == 1);
    }

    // Open WITH erasure
    {
        GQL g3("foo.db", true);
        assert(g3.v().id().empty());
        assert(g3.e().id().empty());

        // Test commits and aborts
        g3.add_vertex(321);
        assert(g3.v().id().size() == 1);

        g3.rollback();
        assert(g3.v().id().empty());

        g3.add_vertex(321);
        assert(g3.v().id().size() == 1);
        g3.commit();
        assert(g3.v().id().size() == 1);
        g3.rollback();
        assert(g3.v().id().size() == 1);
    }
}

void test_creation()
{
    GQL g("foo.db", true);

    g.add_vertex(321).tag("rank", "second");
    g.add_vertex()
        .tag("rank", "first")
        .add_edge(g.v().with_id(321));

    assert(g.v().id().size() == 2);
    assert(g.e().id().size() == 1);
    assert(g.v().with_id(321).id().size() == 1);
}

void test_manager_queries()
{
    GQL g("foo.db", true);
    for (uint64_t i = 0; i < 10; ++i)
    {
        g.add_vertex(i)
            .label(std::to_string(i))
            .add_edge(g.v().with_id(i / 2));
    }

    assert_eq(g.v().where("MOD(id, 2) == 0").id()["id"],
              g.v("MOD(id, 2) == 0").id()["id"]);

    assert_eq(g.e().where("source = target").id()["id"],
              g.e("source = target").id()["id"]);

    g.graphviz("foo.dot");
    assert(system("dot -Tpng foo.dot -o /dev/null") == 0);
}

void test_vertex_queries()
{
    // Setup
    GQL g("foo.db", true);

    g.add_vertex(1).label("first").tag("is_first", "true");
    g.add_vertex(2).label("second");
    g.add_vertex(3).label("third");

    g.v().where("id < 3").add_edge(g.v().where("id > 1"));
    /*
    1 -> 2, 3
    2 -> 2, 3

    1 -> 2>
    |    v
    + -> 3

    id in out
    1  0  2
    2  2  2
    3  2  0
    */

    g.v()
        .with_tag("is_first", "true")
        .complement(g.v())
        .tag("is_first", "false");

    // Where
    assert_eq(g.v().where("id < 2").id()["id"], {"1"});
    assert(g.v().where("id > 3").id().empty());

    // Traversing with labels and tags
    assert_eq(g.v().with_label("second").id()["id"], {"2"});
    assert_eq(g.v().with_tag("is_first", "true").id()["id"],
              {"1"});

    // Join, intersection, complement, excluding
    assert_eq(g.v().id()["id"],
              g.v()
                  .with_tag("is_first", "true")
                  .join(g.v().with_tag("is_first", "false"))
                  .id()["id"]);

    assert(
        g.v()
            .with_tag("is_first", "true")
            .intersection(g.v().with_tag("is_first", "false"))
            .id()
            .empty());

    // In and out sets
    assert_eq(g.v().with_id(1).out().target().id()["id"],
              {"2", "3"});
    assert_eq(g.v().with_id(2).in().source().id()["id"],
              {"1", "2"});

    // In and out degrees
    /*
    id|i |o
    --+--+--
    1 |0 |2
    2 |2 |2
    3 |2 |0
    */
    assert_eq(g.v().with_in_degree(2).id()["id"], {"2", "3"});
    assert_eq(g.v().with_out_degree(2).id()["id"], {"1", "2"});

    // Erasure (erasing nodes nullifies and deletes edges)
    g.v().erase();
    assert(g.v().id().empty() && g.e().id().empty());
}

void test_edge_queries()
{
    GQL g("foo.db", true);

    g.add_vertex().label("alice");
    g.add_vertex().label("bob");
    g.add_vertex().label("pizza");
    g.add_vertex().label("frogs");

    g.v()
        .with_label("alice")
        .add_edge(g.v().with_label("bob"))
        .label("knows");

    g.v()
        .with_label("bob")
        .add_edge(g.v().with_label("alice"))
        .label("knows");

    g.v()
        .with_label("alice")
        .join(g.v().with_label("bob"))
        .add_edge(g.v().with_label("pizza"))
        .label("likes");

    g.v()
        .with_label("bob")
        .add_edge(g.v().with_label("frogs"))
        .label("hates");

    g.v()
        .with_label("alice")
        .add_edge(g.v().with_label("frogs"))
        .label("likes");

    // Where
    assert_eq(g.v()
                  .with_label("pizza")
                  .in()
                  .source()
                  .label()["label"],
              {"alice", "bob"});

    // Erase
    g.e().erase();
    assert(g.e().id().empty());

    // Cross product insertion
    g.add_vertex().label("1");
    g.add_vertex().label("2");
    g.add_vertex().label("3");

    g.add_vertex().label("4");
    g.add_vertex().label("5");

    g.v("label IN ('1', '2', '3')")
        .add_edge(g.v("label IN ('4', '5')"));

    for (const std::string s : {"1", "2", "3"})
    {
        for (const std::string t : {"4", "5"})
        {
            assert(!g.e()
                        .with_source(g.v().with_label(s))
                        .with_target(g.v().with_label(t))
                        .select()
                        .empty());
        }
    }
}

void test_each()
{
    GQL g("foo.db", true);

    for (uint i = 0; i < 10; ++i)
    {
        g.add_vertex(i);
    }

    for (const auto &item : g.v().each())
    {
        assert(item.id()["id"].size() == 1);
    }
}

void test_persistence()
{
    const static std::string f = "foo.db";

    // Create a persistent node
    {
        GQL g(f, true, true);
        g.add_vertex(1234);
    }
    assert(std::filesystem::exists(f));

    // Create a non-persistent node
    {
        GQL g(f, false, false);

        // Assert tables persisted
        assert(!g.v().with_id(1234).id().empty());
    }
    assert(!std::filesystem::exists(f));
}

void test_bounce()
{
    GQL g("foo.db", true);
    uint64_t max = 10000;

    g.add_vertex(1);
    for (uint64_t i = 1; i < max; ++i)
    {
        g.v()
            .with_id(i)
            .add_edge(g.add_vertex(i + 1))
            .label("next");
    }

    // Create and evaluate a copiously long query
    GQL::Vertices query = g.v().with_id(1);
    for (uint64_t i = 1; i < max; ++i)
    {
        query = query.out().with_label("next").target();
    }
    assert(std::stoull(query.id()["id"][0]) == max);
}

void test_keys()
{
    GQL g("foo.db", true);

    g.add_vertex(123)
        .tag("key1", "100")
        .tag("key1", "200")
        .tag("key2", "300")
        .tag("key3", "400");

    const auto keys = g.v().with_id(123).keys()["key"];
    const auto keyset =
        std::set<std::string>(keys.begin(), keys.end());
    assert(keyset.size() == 3);
    assert(keyset.contains("key1"));
    assert(keyset.contains("key2"));
    assert(keyset.contains("key3"));
}

void test_hex()
{
    assert(hex_encode("aB1 !") == "6142312021");

    for (const auto &s :
         {"Hello, world!", "The quick Brown",
          "1 lazy D06 D035 n0t c0un7!!?$#", __FILE__,
          __FILE_NAME__, __FUNCTION__})
    {
        assert(s == hex_decode(hex_encode(s)));
    }
}

////////////////////////////////////////////////////////////////

int main()
{
    std::cout << "Running GQL unit tests...\n";

    // Run tests
    test_merge_rows();
    test_open();
    test_creation();
    test_manager_queries();
    test_vertex_queries();
    test_edge_queries();
    test_each();
    test_limit();
    test_set_operations();
    test_lemma();
    test_multiple_tag_getter();
    test_persistence();
    test_keys();
    test_hex();
    test_bounce();

    // Clean up
    system("rm -f ./foo.*");

    // Notify and exit
    std::cout << "All unit tests passed.\n";
    return 0;
}

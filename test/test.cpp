/*
Unit tests for GQL
*/

#if !(__has_include(<gql.hpp>))
#error "Cannot test before installation"
#endif

#include <cassert>
#include <gql.hpp>
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

    // Join, intersection, complement
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

void test_traversals()
{
    /*
    0 --> 1 --> 2 --> 4 <-> 5
          + --> 3
    */
    GQL g("foo.db", true);
    for (uint64_t i = 0; i <= 5; ++i)
    {
        g.add_vertex(i)
            .label(std::to_string(i))
            .tag("parity", std::to_string(i % 2));
    }
    g.v().with_id(0).add_edge(g.v().with_id(1));
    g.v().with_id(1).add_edge(g.v("id = 2 OR id = 3"));
    g.v().with_id(2).add_edge(g.v().with_id(4));
    g.v().with_id(4).add_edge(g.v().with_id(5));
    g.v().with_id(5).add_edge(g.v().with_id(4));
    g.e().label("foo");
    g.e().where("target = 5").label("fizz");
    g.e().where("target = 4").label("buzz");

    // Forward unconditional
    assert_eq(g.v().with_id(0).traverse().id()["id"],
              {"0", "1", "2", "3", "4", "5"});
    assert_eq(g.v().with_id(2).traverse().id()["id"],
              {"2", "4", "5"});
    assert_eq(g.v().with_id(5).traverse().id()["id"],
              {"4", "5"});

    // Forward edge conditional
    assert_eq(g.v()
                  .with_id(0)
                  .traverse("1", "label != 'fizz'")
                  .id()["id"],
              {"0", "1", "2", "3", "4"});

    // Forward node conditional
    assert_eq(g.v().with_id(0).traverse("id != 4").id()["id"],
              {"0", "1", "2", "3"});

    // Forward both conditional
    assert_eq(g.v()
                  .with_id(0)
                  .traverse("id != 3", "label != 'fizz'")
                  .id()["id"],
              {"0", "1", "2", "4"});

    // Backward unconditional
    assert_eq(g.v().with_id(5).r_traverse().id()["id"],
              {"0", "1", "2", "4", "5"});

    // Backward edge conditional
    assert_eq(g.v()
                  .with_id(5)
                  .r_traverse("1", "label != 'fizz'")
                  .id()["id"],
              {"5"});

    // Backward node conditional
    assert_eq(g.v().with_id(5).r_traverse("id != 1").id()["id"],
              {"2", "4", "5"});

    // Backward both conditional
    assert_eq(g.v()
                  .with_id(5)
                  .r_traverse("id != 1", "label != 'buzz'")
                  .id()["id"],
              {"4", "5"});
}

////////////////////////////////////////////////////////////////

int main()
{
    std::cout << "Running GQL unit tests...\n";

    // Run tests
    test_open();
    test_creation();
    test_manager_queries();
    test_vertex_queries();
    test_edge_queries();
    test_traversals();

    // Clean up
    system("rm -f ./foo.*");

    // Notify and exit
    std::cout << "All unit tests passed.\n";
    return 0;
}

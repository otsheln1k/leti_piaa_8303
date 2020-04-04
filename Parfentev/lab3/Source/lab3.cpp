#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>
#include <functional>

#include <limits.h>

// Note: define ENABLE_DEBUG to enable debugging output to stderr


struct Edge {
    int dest;                   // ‘node_char’s, not indices
    int max_flux, current_flux;
    int rev_idx;                // index of the corresponding dest
                                // node’s edge

    int avail() const { return std::abs(max_flux) - current_flux; }

    // We don’t want to write out auxiliary reverse edges which have
    // negative max_flux
    bool real() const { return max_flux > 0; }
};

struct Node {
    std::vector<Edge> edges;

    Edge &edge(int e) { return edges[e]; }
    const Edge &edge(int e) const { return edges[e]; }

    int edges_count() const { return edges.size(); }
};

// Edge indices, assuming we start from g.start and finish at g.end
using Path = std::vector<int>;

std::ostream & write_node(std::ostream &os, int n);

struct Graph {
    std::vector<Node> nodes;
    int start, end;             // ‘node_char’s

    // We don’t assume any particular range of characters. Instead, we
    // use characters we read as identifiers but only keep nodes in the
    // character range actually used. ‘base_char’ is the lowest
    // character used as a node identifier so far.
    int base_char = -1;

    Graph() :nodes{}, start{-1}, end{-1} {}

    Node &node(int n) { return nodes[n - base_char]; }
    const Node &node(int n) const { return nodes[n - base_char]; }

    // node_char: external id of a node
    int node_char(int idx) const { return idx + base_char; }
    // node_index: zero-based index of node
    int node_index(int n) const { return n - base_char; }

    int nodes_count() const { return nodes.size(); }

    Edge &edge(int n, int e) { return node(n).edge(e); }
    const Edge &edge(int n, int e) const { return node(n).edge(e); }
    Edge &revedge(int n, int e)
    {
        Edge &ed = edge(n, e);
        return edge(ed.dest, ed.rev_idx);
    }
    const Edge &revedge(int n, int e) const
    {
        const Edge &ed = edge(n, e);
        return edge(ed.dest, ed.rev_idx);
    }

    void add_node(int n);

    void add_edge(int n1, int n2, int w);
    void mod_edge(int n, int e, int dw);

    int path_flux(const Path &p) const;
    void apply_flux(const Path &p, int f);

    int get_max_flux();
};

void
Graph::add_node(int n)
{
    if (base_char < 0) {
        // First node
        base_char = n;
    } else if (n < base_char) {
        // Node below lowest char seen before: prepend nodes
        nodes.insert(nodes.begin(), base_char - n, Node{});
        base_char = n;
    } else {
        int idx = n - base_char,
            cur = nodes_count();

        // If new char is above every one seen before, append nodes
        if (idx >= cur)
            nodes.insert(nodes.end(), idx - cur + 1, Node{});
    }
}

void
Graph::add_edge(int n1, int n2, int w)
{
    int e1idx = node(n1).edges_count(),
        e2idx = node(n2).edges_count();

    // Add 2 interconnected (via dest and rev_idx) edges at once.
    node(n1).edges.push_back(Edge{n2, w, 0, e2idx});
    node(n2).edges.push_back(Edge{n1, -w, w, e1idx});

#ifdef ENABLE_DEBUG
    write_node(write_node(std::cerr << "add edge: ", n1), n2);
    write_node(write_node(std::cerr << '/', n2), n1);
    std::cerr << " weight: " << w << std::endl;
#endif
}

void
Graph::mod_edge(int n, int e, int dw)
{
    Edge
        &e1 = edge(n, e),
        &e2 = revedge(n, e);

    e1.current_flux += dw;
    e2.current_flux -= dw;

#ifdef ENABLE_DEBUG
    int d = e1.dest;
    std::cerr << "modify (by " << dw << ") edges: ";
    write_node(write_node(std::cerr, n), d)
        << " (new:" << e1.current_flux << " avail:"
        << e1.avail() << "), ";
    write_node(write_node(std::cerr, d), n)
        << " (new:" << e2.current_flux << " avail:"
        << e2.avail() << ")" << std::endl;
#endif
}

int
Graph::path_flux(const Path &p) const
{
    int f = INT_MAX;

    // Traverse path, find min available flux
    int n = start;
    auto iter = p.begin();
    for (; n != end;
         n = edge(n, *iter++).dest)
        f = std::min(f, edge(n, *iter).avail());

    return f;
}

// ‘apply’: ‘consume’ that much flux from each edge in the path
void
Graph::apply_flux(const Path &p, int f)
{
    int n = start;
    auto iter = p.begin();
    for (; n != end;
         n = edge(n, *iter++).dest)
        mod_edge(n, *iter, f);
}

std::ostream &
debug_write_path(const Graph &g,
                 std::ostream &os,
                 const Path &p)
{
    int n = g.start;
    auto iter = p.begin();
    for (; n != g.end;
         n = g.edge(n, *iter++).dest)
        write_node(os, n);
    write_node(os, n);

    return os;
}

Path find_path(const Graph &g);

int
Graph::get_max_flux()
{
    int total = 0;

    for (;;) {
        Path p = find_path(*this);
        if (p.empty())
            break;

        int f = path_flux(p);
        apply_flux(p, f);
        total += f;

#ifdef ENABLE_DEBUG
        debug_write_path(*this, std::cerr << "found path: ", p);
        std::cerr << ", flux: " << f << std::endl;
        std::cerr << "current total: " << total << std::endl;
#endif
    }

    return total;
}



Path
recover_path(const Graph &g,
             const std::vector<int> &revs)
{
    Path p {};

    // Traverse the path from end to start following recorded reverse
    // edges
    for (int n = g.end; n != g.start;) {
        const Edge &e = g.edge(n, revs[g.node_index(n)]);
        p.push_back(e.rev_idx);
        n = e.dest;
    }

    std::reverse(p.begin(), p.end());

    return p;
}

struct EdgeCmpRef {
    int src;
    int idx;

    const Edge &edge(const Graph &g) const { return g.edge(src, idx); }

    int dst(const Graph &g) const { return edge(g).dest; }
    int rev_idx(const Graph &g) const { return edge(g).rev_idx; }

    // std::less<int> / std::greater<int>
    template<typename C>
    bool compare(const EdgeCmpRef &o,
                 const Graph &g,
                 C cmp) const
    {
        int dst1 = dst(g),
            dst2 = o.dst(g);
        int diff1 = std::abs(dst1 - src),
            diff2 = std::abs(dst2 - o.src);

        if (diff1 != diff2)
            return cmp(diff1, diff2);
        return cmp(dst1, dst2);
    }
};

Path
find_path(const Graph &g)
{
    auto edgecmpref_less =
        [&g](const EdgeCmpRef &a, const EdgeCmpRef &b)
        {
            // std::greater to reverse the std::priority_queue order
            return a.compare(b, g, std::greater<int>{});
        };

    // We’ll always have the best edge on top
    std::priority_queue<EdgeCmpRef,
                        std::vector<EdgeCmpRef>,
                        decltype(edgecmpref_less)> q {edgecmpref_less};

    std::vector<bool> visited (g.nodes_count(), false);

    // rev[g.node_index(n)]: which edge of node n we should use to
    // return to the node from which we came to ‘n’ the first time
    std::vector<int> rev (g.nodes_count(), -1);

    int n = g.start;
    while (n != g.end) {
        visited[g.node_index(n)] = true;

        int ec = g.node(n).edges_count();
        for (int ei = 0; ei < ec; ei++) {
            const Edge &e = g.edge(n, ei);
            if (!visited[g.node_index(e.dest)]
                && e.avail() > 0) {
                q.push(EdgeCmpRef{n, ei});

#ifdef ENABLE_DEBUG
                write_node(std::cerr << " pushing edge: ", n) << " -> ";
                write_node(std::cerr, g.edge(n, ei).dest) << std::endl;
#endif
            }
        }

        // No more edges but we haven’t seen g.end => no path
        if (q.empty())
            return Path{};

        // Pick the next edge
        // n <- next node
        // r <- reverse edge
        int r;
        do {
            const EdgeCmpRef &er = q.top();

            r = er.rev_idx(g);
            n = er.dst(g);

            q.pop();

#ifdef ENABLE_DEBUG
            write_node(std::cerr << " looking at edge: ", er.src);
            write_node(std::cerr << " -> ", n) << std::endl;
#endif
        } while (visited[g.node_index(n)]);

#ifdef ENABLE_DEBUG
        std::cerr << " ok\n";
#endif

        rev[g.node_index(n)] = r;
    }

    return recover_path(g, rev);
}



// Read a node character and make sure it’s valid in the graph
int
read_node(std::istream &is,
          Graph &g)
{
    char c;
    is >> c;

    g.add_node(c);

    return c;
}

Graph
read_graph(std::istream &is)
{
    Graph g {};

    int count;
    is >> count;

    g.start = read_node(is, g);
    g.end = read_node(is, g);

    for (int i = 0; i < count; i++) {
        int n1 = read_node(is, g),
            n2 = read_node(is, g),
            weight;
        is >> weight;

        g.add_edge(n1, n2, weight);
    }

    return g;
}



std::ostream &
write_node(std::ostream &os, int n)
{
    return os << static_cast<char>(n);
}

std::ostream &
write_edge(std::ostream &os,
           const Graph &g, int n, int e)
{
    // Format: "{from} {to} {actual_flux}"
    const Edge &edge = g.edge(n, e);
    return write_node(write_node(os, n) << " ", edge.dest)
        << " " << edge.current_flux << std::endl;
}

// Write edges sorted by destination node character
std::ostream &
write_node_edges(std::ostream &os,
                 const Graph &g, int n)
{
    struct EdgeRef {
        int dest;
        int idx;

        bool operator<(const EdgeRef &er) const
        {
            return dest < er.dest;
        }
    };
    std::vector<EdgeRef> ers {};

    int c = g.node(n).edges_count();
    for (int i = 0; i < c; i++) {
        const Edge &edge = g.edge(n, i);
        if (edge.real())
            ers.push_back(EdgeRef{edge.dest, i});
    }

    std::sort(ers.begin(), ers.end());

    for (const EdgeRef &er : ers)
        write_edge(os, g, n, er.idx);

    return os;
}

std::ostream &
write_flux(std::ostream &os,
           const Graph &g)
{
    // Note: we iterate over node indices, so we use g.node_char
    int c = g.nodes_count();
    for (int i = 0; i < c; i++)
        write_node_edges(os, g, g.node_char(i));
    return os;
}



int
main(void)
{
    Graph g = read_graph(std::cin);
    int flux = g.get_max_flux();
    write_flux(std::cout << flux << std::endl, g);

    return 0;
}

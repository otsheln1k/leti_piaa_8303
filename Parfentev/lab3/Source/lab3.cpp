#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>
#include <functional>

#include <limits.h>

// Note: define ENABLE_DEBUG to enable debugging output to stderr


// Id, local to a particular Vertex. Might be an iterator type if Edges
// are kept in a container whose insertion doesn’t invalidate them.
using edge_id_t = int;

struct Edge {
    int dest;                   // ‘vertex_char’s, not indices
    int max_capacity, current_flux;
    edge_id_t rev;              // id of the corresponding dest’s edge

    int remaining_capacity() const
    {
        return std::abs(max_capacity) - current_flux;
    }

    // We don’t want to write out auxiliary reverse edges which have
    // negative max_capacity
    bool is_real() const
    {
        return max_capacity > 0;
    }
};

class Vertex {
    std::vector<Edge> edges;

public:
    Edge &edge(edge_id_t e)
    {
        return edges[e];
    }
    const Edge &edge(edge_id_t e) const
    {
        return edges[e];
    }

    edge_id_t add_edge()
    {
        edges.push_back(Edge{});
        return edges.size() - 1;
    }

    edge_id_t begin_id() const
    {
        return 0;
    }
    edge_id_t end_id() const
    {
        return edges.size();
    }
};

// Edge indices, assuming we start from g.start and finish at g.end
using Path = std::vector<edge_id_t>;

std::ostream &write_vertex(std::ostream &os, int v);

struct Graph {
    std::vector<Vertex> vertexes;
    int start, end;             // ‘vertex_char’s

    // We don’t assume any particular range of characters. Instead, we
    // use characters we read as identifiers but only keep vertexes in
    // the character range actually used. ‘base_char’ is the lowest
    // character used as a vertex identifier so far.
    int base_char = -1;

    Graph() :vertexes{}, start{-1}, end{-1} {}

    Vertex &vertex(int v)
    {
        return vertexes[v - base_char];
    }
    const Vertex &vertex(int v) const
    {
        return vertexes[v - base_char];
    }

    // vertex_char: external id of a vertex
    int vertex_char(int idx) const
    {
        return idx + base_char;
    }
    // vertex_index: zero-based index of vertex
    int vertex_index(int v) const
    {
        return v - base_char;
    }

    int vertexes_count() const
    {
        return vertexes.size();
    }

    Edge &edge(int v, edge_id_t e)
    {
        return vertex(v).edge(e);
    }
    const Edge &edge(int v, edge_id_t e) const
    {
        return vertex(v).edge(e);
    }
    Edge &revedge(int v, edge_id_t e)
    {
        Edge &ed = edge(v, e);
        return edge(ed.dest, ed.rev);
    }
    const Edge &revedge(int v, edge_id_t e) const
    {
        const Edge &ed = edge(v, e);
        return edge(ed.dest, ed.rev);
    }

    void add_vertex(int v);

    void add_edge(int v1, int v2, int cap);
    void mod_edge(int v, edge_id_t e, int dcap);

    int path_flux(const Path &p) const;
    void apply_flux(const Path &p, int f);

    int get_max_flux();

    Path recover_path(const std::vector<edge_id_t> &revs) const;
    Path find_path() const;
};

void
Graph::add_vertex(int v)
{
    if (base_char < 0) {
        // First vertex
        base_char = v;
    } else if (v < base_char) {
        // Vertex below lowest char seen before: prepend vertexs
        vertexes.insert(vertexes.begin(), base_char - v, Vertex{});
        base_char = v;
    } else {
        int idx = vertex_index(v),
            cur = vertexes_count();

        // If new char is above every one seen before, append vertexes
        if (idx >= cur)
            vertexes.insert(vertexes.end(), idx - cur + 1, Vertex{});
    }
}

void
Graph::add_edge(int v1, int v2, int cap)
{
    edge_id_t
        e1 = vertex(v1).add_edge(),
        e2 = vertex(v2).add_edge();

    // Add 3 interconnected (via dest and rev) edges at once.
    edge(v1, e1) = Edge{v2, cap, 0, e2};
    edge(v2, e2) = Edge{v1, -cap, cap, e1};

    #ifdef ENABLE_DEBUG
    write_vertex(write_vertex(std::cerr << "add edge: ", v1), v2);
    write_vertex(write_vertex(std::cerr << '/', v2), v1);
    std::cerr << " max capacity: " << cap << std::endl;
    #endif
}

void
Graph::mod_edge(int v, edge_id_t e, int dcap)
{
    Edge
        &e1 = edge(v, e),
        &e2 = revedge(v, e);

    e1.current_flux += dcap;
    e2.current_flux -= dcap;

    #ifdef ENABLE_DEBUG
    int d = e1.dest;
    std::cerr << "modify (by " << dcap << ") edges: ";
    write_vertex(write_vertex(std::cerr, v), d)
        << " (new:" << e1.current_flux << " remaining capacity:"
        << e1.remaining_capacity() << "), ";
    write_vertex(write_vertex(std::cerr, d), v)
        << " (new:" << e2.current_flux << " remaining capacity:"
        << e2.remaining_capacity() << ")" << std::endl;
    #endif
}

int
Graph::path_flux(const Path &p) const
{
    int f = INT_MAX;

    // Traverse path, find min remaining capacity
    int v = start;
    auto iter = p.begin();
    for (; v != end; v = edge(v, *iter++).dest)
        f = std::min(f, edge(v, *iter).remaining_capacity());

    return f;
}

// ‘apply’: ‘consume’ that much flux from each edge in the path
void
Graph::apply_flux(const Path &p, int f)
{
    int v = start;
    auto iter = p.begin();
    for (; v != end; v = edge(v, *iter++).dest)
        mod_edge(v, *iter, f);
}

std::ostream &
debug_write_path(const Graph &g,
                 std::ostream &os,
                 const Path &p)
{
    int v = g.start;
    auto iter = p.begin();
    for (; v != g.end; v = g.edge(v, *iter++).dest)
        write_vertex(os, v);
    write_vertex(os, v);

    return os;
}

int
Graph::get_max_flux()
{
    int total = 0;

    for (;;) {
        Path p = find_path();
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
Graph::recover_path(const std::vector<edge_id_t> &revs) const
{
    Path p {};

    // Traverse the path from end to start following recorded reverse
    // edges
    for (int v = end; v != start;) {
        const Edge &e = edge(v, revs[vertex_index(v)]);
        p.push_back(e.rev);
        v = e.dest;
    }

    std::reverse(p.begin(), p.end());

    return p;
}

Path
Graph::find_path() const
{
    // Specifies a particular edge in the graph. Comparison (operator<)
    // is defined to make the best edge the largest.
    struct ComparedEdgeRef {
        // required to calculate distance between dest and source
        int src;

        // if we have an edge ptr, we don’t need a ref to the graph
        const Edge *edge;

        int dst() const
        {
            return edge->dest;
        }
        edge_id_t rev() const
        {
            return edge->rev;
        }

        bool operator<(const ComparedEdgeRef &o) const
        {
            int dst1 = dst(),
                dst2 = o.dst();
            int diff1 = std::abs(dst1 - src),
                diff2 = std::abs(dst2 - o.src);

            // Note: using ‘>’ (greater) here to keep the edge with the
            // smallest distance or destination vertex id the largest
            // one.
            if (diff1 != diff2)
                return diff1 > diff2;

            // here as well
            return dst1 > dst2;
        }
    };

    // We’ll always have the best edge on top
    std::priority_queue<ComparedEdgeRef> q {};

    std::vector<bool> visited (vertexes_count(), false);

    // rev[vertex_index(v)]: which edge of vertex v we should use to
    // return to the vertex from which we came to ‘v’ the first time
    std::vector<int> rev (vertexes_count(), -1);

    int v = start;
    while (v != end) {
        visited[vertex_index(v)] = true;

        const Vertex &vv = vertex(v);
        for (edge_id_t ei = vv.begin_id(); ei != vv.end_id(); ++ei) {
            const Edge &e = vv.edge(ei);

            if (!visited[vertex_index(e.dest)]
                && e.remaining_capacity() > 0) {

                q.push(ComparedEdgeRef{v, &e});

                #ifdef ENABLE_DEBUG
                write_vertex(std::cerr << " pushing edge: ", v)
                    << " -> ";
                write_vertex(std::cerr, edge(v, ei).dest) << std::endl;
                #endif
            }
        }

        // No more edges but we haven’t seen end => no path
        if (q.empty())
            return Path{};

        // Pick the next edge
        // v <- next vertex
        // r <- reverse edge
        edge_id_t r;
        do {
            const ComparedEdgeRef &er = q.top();

            r = er.rev();
            v = er.dst();

            q.pop();

            #ifdef ENABLE_DEBUG
            write_vertex(std::cerr << " looking at edge: ", er.src);
            write_vertex(std::cerr << " -> ", v) << std::endl;
            #endif

        } while (visited[vertex_index(v)]);

        #ifdef ENABLE_DEBUG
        std::cerr << " ok" << std::endl;
        #endif

        rev[vertex_index(v)] = r;
    }

    return recover_path(rev);
}



// Read a vertex character and make sure it’s valid in the graph
int
read_vertex(std::istream &is, Graph &g)
{
    char c;
    is >> c;

    g.add_vertex(c);

    return c;
}

Graph
read_graph(std::istream &is)
{
    Graph g {};

    int count;
    is >> count;

    g.start = read_vertex(is, g);
    g.end = read_vertex(is, g);

    for (int i = 0; i < count; i++) {
        int v1 = read_vertex(is, g),
            v2 = read_vertex(is, g);

        int max_capacity;
        is >> max_capacity;

        g.add_edge(v1, v2, max_capacity);
    }

    return g;
}



std::ostream &
write_vertex(std::ostream &os, int v)
{
    return os << static_cast<char>(v);
}

std::ostream &
write_edge(std::ostream &os,
           const Graph &g, int v, edge_id_t e)
{
    // Format: "{from} {to} {actual_flux}"
    const Edge &edge = g.edge(v, e);
    return write_vertex(write_vertex(os, v) << " ", edge.dest)
        << " " << edge.current_flux << std::endl;
}

// Write edges sorted by destination vertex character
std::ostream &
write_vertex_edges(std::ostream &os,
                   const Graph &g, int v)
{
    struct EdgeRef {
        int dest;
        edge_id_t ei;

        bool operator<(const EdgeRef &er) const
        {
            return dest < er.dest;
        }
    };
    std::vector<EdgeRef> ers {};

    const Vertex &vv = g.vertex(v);
    for (edge_id_t ei = vv.begin_id(); ei != vv.end_id(); ++ei) {
        const Edge &edge = vv.edge(ei);
        if (edge.is_real())
            ers.push_back(EdgeRef{edge.dest, ei});
    }

    std::sort(ers.begin(), ers.end());

    for (const EdgeRef &er : ers)
        write_edge(os, g, v, er.ei);

    return os;
}

std::ostream &
write_flux(std::ostream &os, const Graph &g)
{
    // Note: we iterate over vertex indices, so we use g.vertex_char
    int c = g.vertexes_count();
    for (int i = 0; i < c; i++)
        write_vertex_edges(os, g, g.vertex_char(i));
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

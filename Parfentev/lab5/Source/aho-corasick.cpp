#include <vector>
#include <string>
#include <queue>
#include <iostream>
#include <stack>
#include <algorithm>
#include <map>


struct Result {
    size_t pat_idx;
    size_t length;
    Result *next;
};

struct State;

struct Transition {
    char ch;
    State *dest;
};

struct State {
    std::vector<Transition> transitions {};
    State *fallback = nullptr;
    Result *results = nullptr;
    Result **results_back {&results};
};

#ifdef ENABLE_DEBUG
std::ostream &
operator<<(std::ostream &os, State *stt)
{
    static std::map<State *, int> state_ids {};
    static int next_state_id = 0;

    if (!stt) {
        return os << "(null)";
    }

    auto iter = state_ids.find(stt);
    int id;
    if (iter == state_ids.end()) {
        id = state_ids[stt] = next_state_id++;
    } else {
        id = iter->second;
    }

    return os << id;
}
#endif

void
addResult(State *stt, size_t pat_idx, size_t length)
{
    auto *nr = new Result {pat_idx, length, nullptr};
    *stt->results_back = nr;
    stt->results_back = &nr->next;
}

Transition *
findTransition(State *stt, char ch)
{
    for (Transition &t: stt->transitions) {
        if (t.ch == ch) {
            return &t;
        }
    }
    return nullptr;
}

void
extendForest(State *root, const char *chars, size_t len, size_t idx)
{
    State *s = root;
    for (size_t i = 0; i < len; ++i) {
        char c = chars[i];
        if (auto *t = findTransition(s, c)) {
            s = t->dest;

#ifdef ENABLE_DEBUG
            std::cerr << "\tFound transition for `" << c
                      << "' to state " << s << "\n";
#endif
        } else {
            auto *ns = new State {};
            s->transitions.push_back(Transition{c, ns});
            s = ns;

#ifdef ENABLE_DEBUG
            std::cerr << "\tNo transition for `" << c
                      << "'; new state " << s << "\n";
#endif
        }
    }

    addResult(s, idx, len);
}

State *
buildForest(std::vector<std::string> patterns)
{
    auto *root = new State {};

#ifdef ENABLE_DEBUG
    std::cerr << "Root state " << root << "\n";
#endif

    for (size_t i = 0; i < patterns.size(); ++i) {
        const std::string &pat = patterns[i];

#ifdef ENABLE_DEBUG
        std::cerr << "Building nodes for pattern \"" << pat << "\"\n";
#endif

        extendForest(root, pat.c_str(), pat.length(), i);
    }

    return root;
}

State *
findFallback(State *parent, char ch)
{
    for (State *p = parent; p != nullptr; p = p->fallback) {
        if (auto *t = findTransition(p, ch)) {
            return t->dest;
        }
    }
    return nullptr;
}

void
forestIntoStateMachine(State *root)
{
    std::queue<State *> q {{root}};

#ifdef ENABLE_DEBUG
    std::cerr << "Processing forest with root " << root << "\n";
#endif

    while (!q.empty()) {
        State *s = q.front();
        q.pop();

        State *sf = s->fallback;

#ifdef ENABLE_DEBUG
        std::cerr << "Processing state " << s
                  << " with fallback " << sf << "\n";
#endif

        for (auto &t: s->transitions) {
            State *d = t.dest;
            q.push(d);

            State *df = findFallback(sf, t.ch);

            // df == nullptr => no match on root, so no suffix matches
            // any pattern’s prefix, so we’ll try a null prefix -- that
            // is, fall back to root.
            d->fallback = df ? df : root;

            // ‘attach’ fallback’s result list to ours so that we don’t
            // have to inspect fallbacks all the way down on match.
            *d->results_back = d->fallback->results;

#ifdef ENABLE_DEBUG
            std::cerr << "\tFallback for state " << d
                      << " (transition on `" << t.ch << "')"
                      << ": " << d->fallback << "\n";
#endif
        }
    }
}

void
destroyStateMachine(State *root)
{
    std::queue<State *> q {{root}};

    while (!q.empty()) {
        State *s = q.front();
        q.pop();

        for (auto &t: s->transitions) {
            q.push(t.dest);
        }

        // only keep our own results
        *s->results_back = nullptr;

        // there may be duplicate patterns, so multiple own results in
        // one node
        for (Result *n, *r = s->results; r; r = n) {
            n = r->next;
            delete r;
        }

        delete s;
    }
}



bool
stepMatching(State **cs, char c)
{
#ifdef ENABLE_DEBUG
    std::cerr << "Looking for transition for `" << c << "'\n";
#endif

    for (;;) {
        if (auto *t = findTransition(*cs, c)) {
#ifdef ENABLE_DEBUG
            std::cerr << "\tFound transition from " << *cs
                      << " to " << t->dest << "\n";
#endif

            *cs = t->dest;
            return true;
        } else if ((*cs)->fallback) {
#ifdef ENABLE_DEBUG
            std::cerr << "\tNo transition from " << *cs
                      << "; fallback to " << (*cs)->fallback << "\n";
#endif

            *cs = (*cs)->fallback;
        } else {
#ifdef ENABLE_DEBUG
            std::cerr << "\tNo transition from root; exiting\n";
#endif

            return false;
        }
    }
}

struct Match {
    size_t pat_idx;
    size_t start_idx;

    bool operator<(const Match &o) const
    {
        if (start_idx == o.start_idx)
            return pat_idx < o.pat_idx;
        return start_idx < o.start_idx;
    }
};

std::vector<Match>
getMatches(State *root, const std::string &str)
{
    std::vector<Match> matches {};
    State *cs = root;

#ifdef ENABLE_DEBUG
    std::cerr << "Looking for matches in string \"" << str << "\"\n";
#endif

    for (size_t i = 0; i < str.size(); ++i) {
        if (stepMatching(&cs, str[i])) {
            for (Result *r = cs->results; r; r = r->next) {
                size_t start = i - r->length + 1;
                matches.push_back(Match{r->pat_idx, start});

#ifdef ENABLE_DEBUG
                std::cerr << "Found pattern #" << r->pat_idx
                          << " starting at " << (start + 1) << "\n";
#endif
            }
        }
    }

    return matches;
}



std::ostream &
writeStateMachine(std::ostream &os, State *root)
{
    std::stack<State *> stk {{root}};

    while (!stk.empty()) {
        State *s = stk.top();
        stk.pop();

        os << "State " << s << ":\n";
        for (auto &t: s->transitions) {
            stk.push(t.dest);
            os << "\tTransition on '" << t.ch
               << "' to " << t.dest << "\n";
        }

        os << "\tFallback to " << s->fallback << "\n";

        for (Result *r = s->results; r; r = r->next) {
            os << "\tResult #" << r->pat_idx
               << " of length " << r->length << "\n";
        }
    }

    return os;
}

int
main()
{
    std::string search_s;
    std::getline(std::cin, search_s);

    int n_patterns;
    std::cin >> n_patterns >> std::ws;

    std::vector<std::string> patterns;
    patterns.resize(n_patterns);
    for (int i = 0; i < n_patterns; i++) {
        std::getline(std::cin, patterns[i]);
    }

    State *root = buildForest(patterns);
    forestIntoStateMachine(root);

#ifdef ENABLE_DEBUG
    writeStateMachine(std::cerr << "\nState machine:\n", root) << "\n";
#endif

    auto matches = getMatches(root, search_s);

    std::sort(matches.begin(), matches.end());
    for (auto &match: matches) {
        std::cout << (match.start_idx + 1) << " "
                  << (match.pat_idx + 1) << "\n";
    }

    destroyStateMachine(root);

    return 0;
}

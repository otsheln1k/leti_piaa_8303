#include <vector>
#include <string>
#include <queue>
#include <iostream>
#include <stack>
#include <algorithm>


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



struct PartInfo {
    size_t offset;
    const char *chars;
    size_t length;
};

struct Complement {
    size_t index;
    char ch;
};

struct Pattern {
    std::vector<PartInfo> parts;
    std::vector<Complement> complement;
    size_t length;
};

Pattern
createPatternStructure(const std::string &pat,
                       char wildcard, char complement)
{
    std::vector<PartInfo> parts;
    std::vector<Complement> compls;

    size_t extra_offset = 0;

    const char
        *hd = pat.c_str(),
        *pc = hd;
    while (pc < hd + pat.size()) {
        const char *end = pc;
        for (char c = *end;; c = *++end) {
            if (c == 0
                || c == wildcard
                || c == complement) {
                break;
            }
        }

        if (end != pc) {
            size_t
                offset = pc - hd - extra_offset,
                length = end - pc;
            parts.push_back(PartInfo{offset, pc, length});

#ifdef ENABLE_DEBUG
            std::cerr << "Part at offset " << offset
                      << " of length " << length
                      << ": \"";
            std::cerr.write(pc, length) << "\"\n";
#endif
        }

        pc = end;
        if (complement
            && *pc == complement) {
            size_t offset = pc - hd - extra_offset;
            char ch = pc[1];
            compls.push_back(Complement{offset, ch});

#ifdef ENABLE_DEBUG
            std::cerr << "Complement to char `" << ch
                      << "' at offset " << offset << "\n";
#endif

            pc += 2;
            ++extra_offset;
        }
        for (; *pc == wildcard; ++pc);
    }

    size_t real_size = pat.size() - extra_offset;
    return Pattern{std::move(parts), std::move(compls), real_size};
}

State *
buildForestFromPattern(const Pattern &pat)
{
    auto *root = new State {};

#ifdef ENABLE_DEBUG
    std::cerr << "Root state " << root << "\n";
#endif

    size_t n = pat.parts.size();
    for (size_t i = 0; i < n; ++i) {
        const auto &part = pat.parts[i];

#ifdef ENABLE_DEBUG
        std::cerr << "Building nodes for part \"";
        std::cerr.write(part.chars, part.length) << "\" at offset "
                                                 << part.offset << "\n";
#endif

        extendForest(root, part.chars, part.length, i);
    }

    for (size_t i = 0; i < pat.complement.size(); ++i) {
        const Complement &c = pat.complement[i];

#ifdef ENABLE_DEBUG
        std::cerr << "Building nodes for complement `" << c.ch
                  << "' at offset " << c.index << "\n";
#endif

        extendForest(root, &c.ch, 1, n + i);
    }

    return root;
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

std::vector<size_t>
getTotalMatches(State *root,
                const Pattern &pat,
                const std::string &str)
{
    size_t n_parts = pat.parts.size();
    size_t str_len = str.size();

    std::vector<size_t> partial_matches (pat.length);
    std::vector<bool> disabled (pat.length);
    size_t match_shift = 0;

    std::vector<size_t> matches {};
    State *cs = root;

    for (size_t i = 0; i < str_len; ++i) {
        partial_matches[match_shift] = 0;
        disabled[match_shift] = false;

        if (stepMatching(&cs, str[i])) {
            for (Result *r = cs->results; r; r = r->next) {
                size_t total_off;
                bool complement = r->pat_idx >= n_parts;
                if (complement) {
                    size_t c_idx = r->pat_idx - n_parts;
                    total_off = pat.complement[c_idx].index;
                } else {
                    const PartInfo &part = pat.parts[r->pat_idx];
                    total_off = part.offset + r->length - 1;
                }

                if (i < total_off) {
                    continue;
                }

                size_t start = i - total_off;
                if (start + pat.length > str_len) {
                    continue;
                }

#ifdef ENABLE_DEBUG
                std::cerr << "Part #" << r->pat_idx
                          << " matches at " << (i - r->length + 1)
                          << " (pattern starts at " << start  << ")\n";
#endif

                // -pat.length < match_shift - total_off < pat.length
                size_t idx =
                    (pat.length + match_shift - total_off) % pat.length;

                if (complement) {
                    disabled[idx] = true;

#ifdef ENABLE_DEBUG
                    std::cerr << "Complement found; disabling match at "
                              << start << "\n";
#endif
                } else if (!disabled[idx]) {
                    ++partial_matches[idx];

#ifdef ENABLE_DEBUG
                    std::cerr << partial_matches[idx]
                              << "/" << n_parts
                              << " parts matched at offset "
                              << start << "\n";
#endif
                }
            }
        }

        if (++match_shift == pat.length) {
            match_shift = 0;
        }

        if ((i + 1) >= pat.length
            && !disabled[match_shift]
            && partial_matches[match_shift] == n_parts) {
            size_t start = i + 1 - pat.length;

#ifdef ENABLE_DEBUG
            std::cerr << "Pattern matched at " << start << "\n";
#endif

            matches.push_back(start);
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

std::ostream &
writePatternStructure(std::ostream &os, const Pattern &pat)
{
    for (const auto &part: pat.parts) {
        os << "Part at offset " << part.offset
           << " of length " << part.length
           << ": \"";
        os.write(part.chars, part.length) << "\"\n";
    }

    for (const auto &comp: pat.complement) {
        os << "Complement to char `" << comp.ch
           << "' at index " << comp.index << "\n";
    }

    os << "Total length of pattern: " << pat.length << "\n";

    return os;
}

int
main()
{
    std::string search_s;
    std::getline(std::cin, search_s);

    std::string pattern;
    std::getline(std::cin, pattern);

    char wildcard;
    std::cin >> wildcard;

    char complement;
    if (!(std::cin >> complement)) {
        complement = 0;
    }

    Pattern pat = createPatternStructure(pattern, wildcard, complement);

#ifdef ENABLE_DEBUG
    writePatternStructure(std::cerr << "\nPattern:\n", pat) << "\n";
#endif

    State *root = buildForestFromPattern(pat);
    forestIntoStateMachine(root);

#ifdef ENABLE_DEBUG
    writeStateMachine(std::cerr << "\nState machine:\n", root) << "\n";
#endif

    auto matches = getTotalMatches(root, pat, search_s);

    for (int m: matches) {
        std::cout << (m + 1) << "\n";
    }

    destroyStateMachine(root);

    return 0;
}

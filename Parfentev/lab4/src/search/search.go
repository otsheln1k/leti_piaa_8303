package main

import (
	"io"
	"os"
	"bufio"

	"flag"

	"fmt"
	"log"
	"strings"
)

var verbose bool

// --- KMP algorithm implementation

// n: the string to search in
// np: prefix function computed for n. Only np[:i+1] is ever accessed
// c: character/byte at the current position
// i: first index in ‘n’ in ‘c’ with
func prefixAt(n string, np []int, c byte, i int) int {
	if verbose {
		fmt.Fprintf(os.Stderr, "\tLooking for: '%c'\n", c)
	}
	for {
		if verbose {
			if i >= len(n) {
				fmt.Fprintf(os.Stderr, "\tOut of bounds\n")
			} else {
				fmt.Fprintf(os.Stderr,
					"\tCurrent char: '%c'\n", n[i])
			}
		}

		switch {
		case i < len(n) && c == n[i]:
			if verbose {
				fmt.Fprintf(os.Stderr,
					"\tMatching char at %d\n", i)
			}

			return i + 1
		case i == 0:
			if verbose {
				fmt.Fprintf(os.Stderr, "\tNo match\n")
			}

			return 0
		}
		i = np[i - 1]

		if verbose {
			fmt.Fprintf(os.Stderr, "\tFalling back to %d\n", i)
		}
	}
}

// Compute the prefix function at each offset in ‘s’
func prefix(s string) []int {
	if len(s) == 0 {
		return nil
	}
	prefix := make([]int, len(s))
	prefix[0] = 0
	if verbose {
		fmt.Fprintf(os.Stderr, "At 0: 0 (automatically)\n")
	}
	for i := 1; i < len(s); i++ {
		start := prefix[i - 1]

		if verbose {
			fmt.Fprintf(os.Stderr, "At %d:\n", i)
			fmt.Fprintf(os.Stderr, "\tStarting at %d\n", start)
		}

		prefix[i] = prefixAt(s, prefix, s[i], start)

		if verbose {
			fmt.Fprintf(os.Stderr, "At %d: %d\n", i, prefix[i])
		}
	}
	return prefix
}

// haystack: the string to search in
// needle: the string to search for
// n_prefix: prefix function for each offset in needle
// initial: initial match length. Useful when chaining multiple calls
// offset: the value to add to matches. Useful when splitting search
func findMatches(
	haystack, needle string,
	n_prefix []int,
	initial int,
	offset int,
) ([]int, int) {

	var matches []int
	prev := initial
	n := len(needle)
	for i := 0; i < len(haystack); i++ {
		real_idx := offset + i

		if verbose {
			fmt.Fprintf(os.Stderr, "At %d:\n", real_idx)
			fmt.Fprintf(os.Stderr, "\tPrev = %d\n", prev)
		}

		prev = prefixAt(needle, n_prefix, haystack[i], prev)

		if verbose {
			fmt.Fprintf(os.Stderr, "At %d: %d\n", real_idx, prev)
		}

		if prev == n {
			match_idx := real_idx - n + 1
			matches = append(matches, match_idx)

			if verbose {
				fmt.Fprintf(os.Stderr, "Match at %d-%d\n",
					match_idx, real_idx)
			}
		}
	}
	return matches, prev
}

// --- Parallel implementation

type Range struct {
	offset, length int
}

// len_s: length of haystack
// len_substr: length of needle
// n_ranges: max number of ranges to return
func splitKmpWork(len_s, len_substr, n_ranges int) []Range {
	prematch_area := len_substr - 1

	// match_area: max count of matches
	match_area := len_s - prematch_area

	// In the extreme case, each thread will be checking its own offset.
	if n_ranges > match_area {
		n_ranges = match_area
	}

	//
	match_one_min := match_area / n_ranges
	match_one_rem := match_area % n_ranges

	// Each thread’s match area begins at offset + prematch_area.
	offset := 0
	// ‘+1’: we distribute the remainder (r) among the first r processes.
	match_len := match_one_min + 1

	// Allocate beforehand, so no reallocation
	ranges := make([]Range, 0, n_ranges)
	for i := 0; i < n_ranges; i++ {
		if i == match_one_rem {
			match_len -= 1
		}
		ranges = append(ranges, Range{
			offset,
			match_len + prematch_area,
		})
		offset += match_len
	}

	return ranges
}

func parallelFindMatches(
	haystack, needle string,
	n_prefix []int,
	ranges []Range,
) []int {
	type searchResult struct {
		idx int
		mat []int
	}

	// Spawn a goroutine for each range. Each goroutine sends its index and
	// matches from its range to finish_ch.
	finish_ch := make(chan searchResult)
	for i, rng := range ranges {
		offset := rng.offset
		end := offset + rng.length
		piece := haystack[offset : end]
		go func (i int) {
			m, _ := findMatches(piece, needle, n_prefix, 0, offset)
			finish_ch <- searchResult{i, m}
		}(i)

		if verbose {
			fmt.Fprintf(os.Stderr,
				"Goroutine %d started in range from %d to %d\n",
				i, offset, end)
		}
	}

	// Fetch results. We need matches to be in the correct order, so we put
	// match slices in order.
	results := make([][]int, len(ranges))
	total := 0
	for i := 0; i < len(ranges); i++ {
		res := <-finish_ch
		results[res.idx] = res.mat
		total += len(res.mat)

		if verbose {
			fmt.Fprintf(os.Stderr, "Goroutine %d finished: %v\n",
				res.idx, res.mat)
		}
	}

	if verbose {
		fmt.Fprintf(os.Stderr, "Total: %d matches\n", total)
	}

	// Produce a slice of total matches. We won’t have to reallocate because
	// we specify the required capacity.
	matches := make([]int, 0, total)
	for _, res := range results {
		matches = append(matches, res...)
	}

	return matches
}

func ParallelSearchSubstring(haystack, needle string, n_ranges int) []int {
	if verbose {
		fmt.Fprintf(os.Stderr,
			"Calculating prefix function of search pattern...\n")
	}

	n_prefix := prefix(needle)

	if verbose {
		fmt.Fprintf(os.Stderr,
			"Prefix function of search pattern: %v\n", n_prefix)
	}

	ranges := splitKmpWork(len(haystack), len(needle), n_ranges)
	return parallelFindMatches(haystack, needle, n_prefix, ranges)
}

func printOffsets(off []int) {
	if len(off) == 0 {
		fmt.Println(-1)
		return
	}

	fmt.Print(off[0])
	for _, x := range off[1:] {
		fmt.Printf(",%v", x)
	}
	fmt.Println()
}

func readLine(rd *bufio.Reader) (string, error) {
	str, err := rd.ReadString('\n')
	if err != nil {
		// if err == io.EOF, the string may non-empty and useful
		return str, err
	}

	return strings.TrimSuffix(str, "\n"), nil
}

func main() {
	var n_threads int

	flag.IntVar(
		&n_threads, "j", 1,
		"Number of `threads` to use",
	)
	flag.BoolVar(
		&verbose, "v", false,
		"Show detailed info on algorithm's execution",
	)
	flag.Parse()

	buf_rd := bufio.NewReader(os.Stdin)

	str_a, err := readLine(buf_rd)
	if err != nil {
		log.Fatal(err)
	}

	str_b, err := readLine(buf_rd)
	if err != nil && err != io.EOF {
		log.Fatal(err)
	}

	res := ParallelSearchSubstring(str_b, str_a, n_threads)

	printOffsets(res)
}

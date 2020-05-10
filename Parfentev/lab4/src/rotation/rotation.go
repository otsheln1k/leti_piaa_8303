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
	for {
		switch {
		case i < len(n) && c == n[i]:
			return i + 1
		case i == 0:
			return 0
		}
		i = np[i - 1]
	}
}

// Compute the prefix function at each offset in ‘s’
func prefix(s string) []int {
	if len(s) == 0 {
		return nil
	}
	prefix := make([]int, len(s))
	prefix[0] = 0
	for i := 1; i < len(s); i++ {
		prefix[i] = prefixAt(s, prefix, s[i], prefix[i - 1])
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
		prev = prefixAt(needle, n_prefix, haystack[i], prev)
		real_idx := offset + i

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

// --- Find rotation

// Note: for a non-parallel case, hd would be the same string as tl
func findCyclicMatch(hd, tl, orig string, orig_prefix []int) int {
	n := len(orig)

	_, last := findMatches(hd, orig, orig_prefix, 0, 0)
	switch last {
	case n:
		return 0
	case 0:
		return -1
	}

	matches, _ := findMatches(tl, orig, orig_prefix, last, n)
	if len(matches) != 0 {
		return matches[0]
	} else {
		return -1
	}
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

func ParallelCheckCyclic(str, orig string, n_pieces int) int {
	if (len(str) != len(orig)) {
		return -1
	}

	orig_prefix := prefix(orig)

	if verbose {
		fmt.Fprintf(os.Stderr,
			"Original string's prefix function: %v\n", orig_prefix)
	}

	n := len(orig)
	ranges := splitKmpWork(2*n - 1, n, n_pieces)

	// Spawn a goroutine for each range. Each goroutine will send one number
	// into finish_ch -- either a rotation or -1.
	finish_ch := make(chan int)
	for i, rng := range ranges {
		offset := rng.offset
		end := offset + rng.length - n

		go func () {
			rot := findCyclicMatch(
				str[offset:], str[:end],
				orig, orig_prefix,
			)
			finish_ch <- rot
		}()

		if verbose {
			fmt.Fprintf(os.Stderr,
				"Goroutine %d started in range %d to %d\n",
				i, offset, n+end)
		}
	}

	// Collect results
	res := -1
	for i := 0; i < len(ranges); i++ {
		x := <-finish_ch

		if verbose {
			fmt.Fprintf(os.Stderr, "Goroutine finished: %d\n", x)
		}

		if x >= 0 && (res < 0 || x < res) {
			res = x
			if verbose {
				fmt.Fprintf(os.Stderr,
					"New leftmost result: %d\n", res)
			}
		}
	}
	return res
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

	res := ParallelCheckCyclic(str_a, str_b, n_threads)

	fmt.Println(res)
}

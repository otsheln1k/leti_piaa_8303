SRCDIR = Source
TESTDIR = Tests
EXEFILES = $(SRCDIR)/aho-corasick $(SRCDIR)/wildcard
CXXFLAGS += -std=c++11
OBJFILES = $(addsuffix .o,$(EXEFILES))

all: $(EXEFILES)

debug: all
debug: CXXFLAGS += -DENABLE_DEBUG -g -Wall -Wextra

$(EXEFILES): %: %.o
	$(CXX) -o $@ $^

clean:
	$(RM) $(EXEFILES) $(OBJFILES)


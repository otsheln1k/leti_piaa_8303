SRCDIR = Source
EXEFILE = $(SRCDIR)/lab3
CXXFLAGS += -std=c++11
OBJFILES = $(SRCDIR)/lab3.o

all: $(EXEFILE)

test: $(EXEFILE)
	./run-tests $(EXEFILE) Tests/ --color=auto

debug: all
debug: CXXFLAGS += -DENABLE_DEBUG -g -Wall -Wextra

$(EXEFILE): $(OBJFILES)
	$(CXX) -o $@ $^

clean:
	$(RM) $(EXEFILE) $(OBJFILES)

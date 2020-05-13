SRCDIR=src
EXEFILES=$(SRCDIR)/search/search $(SRCDIR)/rotation/rotation
TESTDIR=tests

all: $(EXEFILES)

.PHONY: all test clean

$(SRCDIR)/search/search: $(SRCDIR)/search/search.go
	cd $(SRCDIR)/search; go build

$(SRCDIR)/rotation/rotation: $(SRCDIR)/rotation/rotation.go
	cd $(SRCDIR)/rotation; go build

test: all
	./test.sh $(SRCDIR)/search/search $(TESTDIR)/search
	./test.sh $(SRCDIR)/rotation/rotation $(TESTDIR)/rotation
	./test.sh $(SRCDIR)/search/search $(TESTDIR)/search -j 4
	./test.sh $(SRCDIR)/rotation/rotation $(TESTDIR)/rotation -j 4

clean:
	$(RM) $(EXEFILES)

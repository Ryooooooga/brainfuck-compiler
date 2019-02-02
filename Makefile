CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Werror
LDFLAGS  :=

.PHONY: all clean

all: bfc

bfc: bfc.o
	${CXX} ${CXXFLAGS} -o $@ $^ ${LDFLAGS}

bfc.o: bfc.hpp

clean:
	${RM} bfc *.o

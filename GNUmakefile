all: attrjager

attrjager: main.o recurse.o handle.o parse_stat.o
	g++ -std=c++14 -g $^ -o $@

%.o: %.cc $(wildcard *.hh)
	g++ -std=c++14 -Wall -Werror -Og -g -c -o $@ $<

clean:
	rm -f attrjager *.o *~ \#*# .#*

.PHONY: all clean
.SECONDARY:

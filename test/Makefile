CXX=g++
CXXFLAGS=-std=c++11 -O2 -g -Wall -lpthread

test_mempool:test_mempool.o
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o:%.cpp ../memory_pool.hpp
	$(CXX) $(CXXFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o test_mempool


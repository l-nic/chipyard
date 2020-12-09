CXXFLAGS+=-std=c++11 -O2

all: make_wiki_index

clean:
	rm -f make_wiki_index

%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

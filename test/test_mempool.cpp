#include <stdio.h>

#include "../memory_pool.hpp"

class Object {
public:
	Object() {
	
	}

	virtual ~Object() {
	
	}
	
	
};

class Child : public Object {
public:
	void setValue(long value) {
		m_value = value;
	}
	
	long getValue() {
		return m_value;
	}

private:
	Child() 
		: m_value(1) {
		//printf("constructing child %p\n", this);
	}
	
	~Child() {
		//printf("destructing child %p\n", this);
	}

	friend class Factory<Child>;

private:
	long m_value;
	char *data;
};

#include <deque>
using namespace std;

struct Array {
	char array[100];
} __attribute__((packed));

int main(int argc, char* argv[])
{
	long capacity = 30000;
	
	Factory<Child> factory(capacity);
	
	deque<Child*> queue;

	int num = 100;
	if (argc == 2) {
		num = atoi(argv[1]);
	}
	
	for (int i = 0; i < num; i++) {
		if (rand() % 2 != 0 && queue.size() < capacity) {
			Child *ptr = factory.produce();
			if (ptr != NULL) {
				queue.push_back(ptr);
			} else {
				printf("Failed to get a new Child due to resource shortage at this time %d. "
					" Already got %d children\n", i, int(queue.size()));
			}
		}
		else if (!queue.empty()) {
			Child *ptr = queue.front();
		//	Child *second = ptr;
			factory.recycle(ptr);
		//	factory.recycle(second);
			queue.pop_front();
		}
		
		if (i % 1000000 == 0) {
			printf("%d: Got %d children\n", i, int(queue.size()));
		}
	}
	
	return 0;
}

// RealTimeResourceManager;

#ifndef BARRIER_H_
#define BARRIER_H_

#include <semaphore.h>

class Barrier {
public:
	Barrier(unsigned int num_of_threads);
	void wait();
	~Barrier();
	int count();

protected:
	sem_t lock;
	sem_t wait_for_all;
	int n, counter;

};

#endif // BARRIER_H_

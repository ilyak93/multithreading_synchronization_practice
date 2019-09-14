#include "Barrier.h"
#include <stdio.h>


Barrier::Barrier(unsigned int num_of_threads) {
	n = num_of_threads;
	counter = 0;
	sem_init(&lock, 0, 1); //to protect critical code
	sem_init(&wait_for_all, 0 ,0); //it will always stay 0, so it will go to wait queue
}

void Barrier::wait() {
	sem_wait(&lock); // only 1 thread can look upon counter, so it's always up to date
	if (counter == n-1){
		counter = 0;
		//sem_post(&lock); // finished looking at the counter, it's the only shared resource
		int i;
		for(i=0; i<n-1; i++){
			sem_post(&wait_for_all); //wake up the others
		}
		int temp = -1;
		sem_getvalue(&wait_for_all, &temp);
	 	while(temp!=0){ //wait until everyone leave the wait queue.
			sem_getvalue(&wait_for_all, &temp);
		}
		sem_post(&lock);
	}
	else{
		counter++; // going to wait
		//printf("counter:%d\n", counter);
		sem_post(&lock); //finished looking at the counter, without this - deadlock because not freeing
		sem_wait(&wait_for_all); // wait
	}
}

Barrier::~Barrier() {
	sem_destroy(&lock);
	sem_destroy(&wait_for_all);
}

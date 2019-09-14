#include "ReceptionHour.h"
#include <stdio.h>

using std::unordered_map;

void* ta_init(void* arg) {
	ReceptionHour* rh = static_cast<ReceptionHour*>(arg);
	//rh->waitForStudent();
	
	bool status = rh->waitForStudent();
	while (status == true){
		status = rh->waitForStudent();
	}
	
	//printf("ta returning\n");
	return NULL;
}

/*
Explaning our work:
When a student is created via the startStudent routine 
he enters student_init routine.
student_init gets the return value from the waitForTeacher
routine and returns it to be collected by pthread_join.

our locks and conditions:
general_lock - to sync between the TA and the students who's currently asking.
door_lock and map_lock - protect our door and map.
student_count_lock -  protect our student_counter that counts when someone enters the room
and decounts when someone leaves the room/
student_lock - a lock to ensure that only one student can ask a question in any given time,
locks in the begining askQuestion, unlocks in waitForAnswer at the end.
left_lock - to ensure that when the door closes students that are still in the office will 
still be given an answer.
we need the left_lock because our TA waits for a signal thats says that a student is ready
to ask a question.
if the user performs closeTheDoor then we lock the door (to ensure no one enters from this point)
and wait until everyone left, only after that we send a signal from the door
to the TA saying the door is locked and there are no more students left.
We do that because there are no more students left and no one could enter - so if not the door
the TA wouldn't wake up.

We checkd our program with helgrind and with -fsanitize=thread, we eliminated every possible data race and lock order violation.

*/

ReceptionHour::ReceptionHour(unsigned int max_waiting_students) {
	max = max_waiting_students;
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&door_lock, &a);
	pthread_mutex_init(&map_lock, &a);
	pthread_mutex_init(&general_lock, &a);
	pthread_mutex_init(&student_lock, &a);
	pthread_mutex_init(&student_count_lock, &a);
	pthread_mutex_init(&left_lock, &a);
	pthread_cond_init(&wake_up_ta, NULL);
	pthread_cond_init(&student_waiting_to_ask, NULL);
	pthread_cond_init(&givenAnswer, NULL);
	pthread_cond_init(&student_has_left, NULL);
	pthread_cond_init(&wake_door, NULL);
	student_has_entred = 0;
	door = true;
	student_count = 0;
	ta_ready_for_question = 0;
	ta_has_answered = 0;
	student_left = 0;
	last_student_has_left = 0;
	pthread_create(&ta, NULL, ta_init, (void*) this);
}

ReceptionHour::~ReceptionHour() {
	
	//printf("des\n");
	pthread_join(ta, NULL);
	pthread_cancel(ta);
	//printf("ta destroyed\n");
}

void* student_init(void* arg) {
	ReceptionHour* rh = static_cast<ReceptionHour*>(arg);
	StudentStatus* status = new StudentStatus;
	*status = rh->waitForTeacher();
	if (*status != StudentStatus::ENTERED){
		return (void *) status;
	}
	rh->askQuestion();
	rh->waitForAnswer();
	return (void *) status;
}

void* student_init_door_closed(void* arg) {
	return (void *) StudentStatus::LEFT_BECAUSE_DOOR_CLOSED;
}

void* student_init_no_seat(void* arg) {
	return (void *) StudentStatus::LEFT_BECAUSE_NO_SEAT;
}

void ReceptionHour::startStudent(unsigned int id) {
	pthread_t student;
	//printf("before create: %lu\n", student);
	pthread_mutex_lock(&map_lock);
	pthread_create(&student, NULL, student_init, (void*) this);
	//printf("after create: %lu\n", student);
	std::pair<unsigned int, pthread_t> pair (id, student);
	map.insert(pair);
	pthread_mutex_unlock(&map_lock);
}

StudentStatus ReceptionHour::finishStudent(unsigned int id) {
	pthread_mutex_lock(&map_lock);
	unordered_map<unsigned int, pthread_t>::const_iterator it = map.find(id);
	if (it == map.end()){
		printf("error\n");
	}
	pthread_mutex_unlock(&map_lock);
	StudentStatus* return_ptr;
	//printf("before join\n");
	pthread_join(it->second, (void**)&return_ptr);
	StudentStatus temp = *return_ptr;
	delete return_ptr;
	pthread_cancel(it->second);
	pthread_mutex_lock(&map_lock);
	map.erase(it);
	pthread_mutex_unlock(&map_lock);
	//printf("student erased\n");
	return temp;
}

void ReceptionHour::closeTheDoor() {
	pthread_mutex_lock(&door_lock);
	door = false;
	pthread_mutex_unlock(&door_lock);

	pthread_mutex_lock(&general_lock);
	pthread_mutex_lock(&student_count_lock);
	if (student_count != 0){
		pthread_mutex_unlock(&student_count_lock);
		while (last_student_has_left == 0){
			pthread_cond_wait(&wake_door, &general_lock);
		}
	}
	else {
		pthread_mutex_unlock(&student_count_lock);
	}
	student_has_entred = 1;
	last_student_has_left = 1;
	pthread_cond_signal(&wake_up_ta);
	pthread_mutex_unlock(&general_lock);
	
	
}

bool ReceptionHour::waitForStudent() {
	pthread_mutex_lock(&door_lock);
	pthread_mutex_lock(&map_lock);
	pthread_mutex_lock(&student_count_lock);
	if (!(door == false && student_count == 0)){
		pthread_mutex_unlock(&map_lock);
		pthread_mutex_unlock(&door_lock);
		pthread_mutex_unlock(&student_count_lock);

		pthread_mutex_lock(&general_lock);
		while (student_has_entred == 0){
			//printf("waiting for to student entr\n");
			pthread_cond_wait(&wake_up_ta, &general_lock);
			//printf("student entred print 1\n");
		}	
		student_has_entred = 0;
		pthread_mutex_lock(&door_lock); 
		pthread_mutex_lock(&student_count_lock);
		if (door == false && student_count == 0){
				pthread_mutex_unlock(&door_lock);
				pthread_mutex_unlock(&general_lock);
				pthread_mutex_unlock(&student_count_lock);
				//printf("door is locked and ta not accepting\n");
				return false;
			}
		pthread_mutex_unlock(&door_lock);
		pthread_mutex_unlock(&student_count_lock);
		pthread_mutex_unlock(&general_lock);
		waitForQuestion();

		
		pthread_mutex_lock(&left_lock);
		//printf("locked left_lock\n");
		//printf("ta: student_left %d\n", student_left);
		while (student_left == 0){
			pthread_cond_wait(&student_has_left, &left_lock);
		}
		student_left = 0;
		pthread_mutex_unlock(&left_lock);
		
		//printf("return to ta\n");	
		return true;
	}
	//printf("door closed\n");
	pthread_mutex_unlock(&student_count_lock);
	pthread_mutex_unlock(&door_lock);
	pthread_mutex_unlock(&map_lock);
	
	return false;
}

void ReceptionHour::waitForQuestion() {
	pthread_mutex_lock(&general_lock);
	//printf("ta ready for question\n");
	ta_ready_for_question = 1;
	pthread_cond_signal(&student_waiting_to_ask);
	pthread_mutex_unlock(&general_lock);
	giveAnswer();
}

void ReceptionHour::giveAnswer() {
	//printf("start giveAnswer\n");
	pthread_mutex_lock(&general_lock);
	//printf("ta gave answer\n");
	ta_has_answered = 1;
	pthread_cond_signal(&givenAnswer);
	pthread_mutex_unlock(&general_lock);
	//printf("finish giveAnswer\n");
}


StudentStatus ReceptionHour::waitForTeacher() {
	pthread_mutex_lock(&door_lock);
	if (door == false){
		pthread_mutex_unlock(&door_lock);
		return StudentStatus::LEFT_BECAUSE_DOOR_CLOSED;
	}
	

	pthread_mutex_lock(&student_count_lock);
	if (student_count == max){
		pthread_mutex_unlock(&student_count_lock);
		pthread_mutex_unlock(&door_lock);
		return StudentStatus::LEFT_BECAUSE_NO_SEAT;
	}
	//printf("count ++ %d\n", student_count);
	student_count++;
	pthread_mutex_unlock(&student_count_lock);
	pthread_mutex_unlock(&door_lock);
	//printf("student entred\n");
	return StudentStatus::ENTERED;
}

void ReceptionHour::askQuestion() {
	pthread_mutex_lock(&student_lock);
	pthread_mutex_lock(&general_lock);
	//printf("student waiting for ta to recive question\n");
	student_has_entred = 1;
	pthread_cond_signal(&wake_up_ta);
	pthread_mutex_unlock(&general_lock);
	pthread_mutex_lock(&general_lock);
	while(ta_ready_for_question == 0){
		//printf("student is waiting for ta to be ready for question\n");
		pthread_cond_wait(&student_waiting_to_ask, &general_lock);
	}
	//printf("student asked\n");
	ta_ready_for_question = 0;
	pthread_mutex_unlock(&general_lock);
}


void ReceptionHour::waitForAnswer() {
	//printf("start waitForAnswer\n");

	pthread_mutex_lock(&general_lock);
	//printf("student waiting for answer\n");
	while(ta_has_answered == 0){
		pthread_cond_wait(&givenAnswer, &general_lock);
	}
	ta_has_answered = 0;
	pthread_mutex_unlock(&general_lock);

	//printf("answered\n");

	
	pthread_mutex_lock(&general_lock);
	pthread_mutex_lock(&student_count_lock);
	student_count--;
	if (student_count == 0){
		last_student_has_left = 1;
		pthread_cond_signal(&wake_door);
		
	}
	pthread_mutex_unlock(&general_lock);
	pthread_mutex_unlock(&student_count_lock);

	//printf("student count-- and lock left\n");

	pthread_mutex_lock(&left_lock);
	student_left = 1;
	//printf("student_left = 1\n");
	pthread_cond_signal(&student_has_left);
	pthread_mutex_unlock(&left_lock);
	//printf("student done asking\n");
	pthread_mutex_unlock(&student_lock);
	//printf("finish waitForAnswer\n");
}


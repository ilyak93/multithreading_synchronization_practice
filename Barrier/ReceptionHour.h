#ifndef RECEPTIONHOUR_H_
#define RECEPTIONHOUR_H_

#include <unordered_map>
#include <pthread.h>

using std::unordered_map;

enum StudentStatus {
	ENTERED = 0,
	LEFT_BECAUSE_NO_SEAT,
	LEFT_BECAUSE_DOOR_CLOSED
};

class ReceptionHour {
public:
	ReceptionHour(unsigned int max_waiting_students);
	 ~ReceptionHour();

	 void startStudent(unsigned int id);
	 StudentStatus finishStudent(unsigned int id);

	 void closeTheDoor();

	 bool waitForStudent();
	 void waitForQuestion();
	 void giveAnswer();

	 StudentStatus waitForTeacher();
	 void askQuestion();
	 void waitForAnswer();

	//unsigned int getMax();
	//unsigned int getCurrent();
	//void currentPlus();


protected:
	bool door;
	unsigned int max;
	unsigned int student_count;
	unordered_map<unsigned int, pthread_t> map;
	pthread_mutex_t map_lock;
	pthread_mutex_t door_lock;
	 pthread_t ta;
	pthread_mutex_t general_lock;
	pthread_mutex_t student_lock;
	pthread_mutex_t student_count_lock;
	pthread_mutex_t left_lock;
	pthread_cond_t wake_up_ta;
	pthread_cond_t student_waiting_to_ask;
	pthread_cond_t givenAnswer;
	pthread_cond_t student_has_left;
	pthread_cond_t wake_door;
	int student_has_entred;
	int ta_ready_for_question;
	int ta_has_answered;
	int student_left;
	int last_student_has_left;

};

#endif // RECEPTIONHOUR_H_

#include "Factory2.h"

struct aux {
	Factory* ths;
	int aux_data;
	unsigned int aux_data2;
	Product* aux_array;
};

void* prod_init(void* arg) {
	aux* a = static_cast<aux*>(arg);
	Factory* fact = a->ths;
	fact->produce(a->aux_data, a->aux_array);
	return NULL;
}

void* simple_buyer_init(void* arg) {
	aux* a = static_cast<aux*>(arg);
	Factory* fact = a->ths;
	fact->tryBuyOne();
	return NULL;
}

void* company_init(void* arg) {
	aux* a = static_cast<aux*>(arg);
	Factory* fact = a->ths;
	std::list<Product> products = fact->buyProducts(a->aux_data2);
	int min_value = a->aux_data;
	std::list<Product> products_to_return;
	for(std::list<Product>::iterator it = products.begin(); it != products.end(); ++it){
		if((*it).getValue() < min_value){
			products_to_return.push_back(*it);
		}
	}

	fact->returnProducts(products_to_return, a->aux_data2);
	return NULL;
}

void* thief_init(void* arg) {
	aux* a = static_cast<aux*>(arg);
	Factory* fact = a->ths;
	fact->stealProducts(a->aux_data, a->aux_data2);
	return NULL;
}

Factory::Factory(){
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&avail_lock, &a);
	pthread_mutex_init(&map_lock, &a);
	pthread_cond_init(&company_in, NULL);
	door_open = true;
    returning_service_open = true;
}

Factory::~Factory(){
	
}

void Factory::startProduction(int num_products, Product* products, unsigned int id){
	pthread_t pt;
	aux tmp = {this, num_products, 2, products};
	pthread_create(&pt, NULL, prod_init, (void*) (&tmp));
	pthread_mutex_lock(&map_lock);
	std::pair<unsigned int, pthread_t> pair(id, pt);
	prod_thrds.insert(pair);
	pthread_mutex_unlock(&map_lock);
}

void Factory::produce(int num_products, Product* products){
	pthread_mutex_lock(&avail_lock);
	for(int i = 0; i < num_products; ++i) {
		avail.push_front(products[i]);
	}
	pthread_mutex_unlock(&avail_lock);
}

void Factory::finishProduction(unsigned int id){
	std::map<unsigned int,pthread_t>::iterator it;
	pthread_mutex_lock(&map_lock);
	it = prod_thrds.find(id);
	pthread_t pt = (*it).second;
	pthread_join(pt, NULL);
	pthread_cancel(pt);
    prod_thrds.erase(it);
	pthread_mutex_unlock(&map_lock);
}

void Factory::startSimpleBuyer(unsigned int id){
	pthread_t pt;
	aux tmp = {this, 0, 0, NULL};
	pthread_create(&pt, NULL, simple_buyer_init, (void*) (&tmp));
	pthread_mutex_lock(&map_lock);
	std::pair<unsigned int, pthread_t> pair(id, pt);
	prod_thrds.insert(pair);
	pthread_mutex_unlock(&map_lock);
	
}

int Factory::tryBuyOne(){
	int res = pthread_mutex_trylock(&avail_lock);
	if(!res || avail.size() == 0 || door_open == false) return -1;
	Product tmp = avail.back();
	avail.pop_back();
	pthread_mutex_unlock(&avail_lock);
    return tmp.getId();;
}

int Factory::finishSimpleBuyer(unsigned int id){
    std::map<unsigned int,pthread_t>::iterator it;
	pthread_mutex_lock(&map_lock);
	it = prod_thrds.find(id);
	if (it != prod_thrds.end()) return -1;
	pthread_t pt = (*it).second; 
	pthread_join(pt, NULL);
	pthread_cancel(pt);
    prod_thrds.erase(it);
	pthread_mutex_unlock(&map_lock);
	
	return id;
}

void Factory::startCompanyBuyer(int num_products, int min_value, unsigned int id){
	pthread_t pt;
	aux tmp = {this, min_value, num_products, NULL};
	pthread_create(&pt, NULL, company_init, (void*) (&tmp));
	pthread_mutex_lock(&map_lock);
	std::pair<unsigned int, pthread_t> pair(id, pt);
	prod_thrds.insert(pair);
	pthread_mutex_unlock(&map_lock);
}

std::list<Product> Factory::buyProducts(int num_products){
	
	pthread_mutex_lock(&avail_lock);
	while(door_open == false){
		pthread_mutex_unlock(&avail_lock);
		pthread_mutex_lock(&avail_lock);
	}
	pthread_mutex_unlock(&avail_lock);
	
	pthread_cond_wait(&company_in, &avail_lock);
	
	pthread_mutex_lock(&avail_lock);
	while(avail.size() < num_products){
		pthread_mutex_unlock(&avail_lock);
		pthread_mutex_lock(&avail_lock);
	}
	
	pthread_mutex_unlock(&avail_lock);
	
	std::list<Product> res;
	pthread_mutex_lock(&avail_lock);
	for(int i = 0; i < num_products; ++i){
		res.push_front(avail.back());
		avail.pop_back();
	}
	pthread_mutex_unlock(&avail_lock);
	
	pthread_cond_broadcast(&company_in);
	
    return res;
}

void Factory::returnProducts(std::list<Product> products, unsigned int id){ // why id

	pthread_mutex_lock(&avail_lock);
	while(door_open == false){
		pthread_mutex_unlock(&avail_lock);
		pthread_mutex_lock(&avail_lock);
	}
	pthread_mutex_unlock(&avail_lock);
	
	while(returning_service_open == false){
		pthread_mutex_unlock(&avail_lock);
		pthread_mutex_lock(&avail_lock);
	}
	pthread_mutex_unlock(&avail_lock);
	
	pthread_cond_wait(&company_in, &avail_lock);
	
	pthread_mutex_lock(&avail_lock);
	for(std::list<Product>::iterator it = products.begin(); it != products.end(); ++it){
		avail.push_front(*it);
	}
	pthread_mutex_unlock(&avail_lock);
	
	pthread_cond_broadcast(&company_in);
}

int Factory::finishCompanyBuyer(unsigned int id){
    std::map<unsigned int,pthread_t>::iterator it;
	pthread_mutex_lock(&map_lock);
	it = prod_thrds.find(id);
	if (it == prod_thrds.end()) return -1;
	pthread_t pt = (*it).second; 
	pthread_join(pt, NULL);
	pthread_cancel(pt);
    prod_thrds.erase(it);
	pthread_mutex_unlock(&map_lock);
	
	return id;
}


void Factory::startThief(int num_products,unsigned int fake_id){
	pthread_t pt;
	aux tmp = {this, num_products, fake_id, NULL};
	pthread_create(&pt, NULL, thief_init, (void*) (&tmp));
	pthread_mutex_lock(&map_lock);
	std::pair<unsigned int, pthread_t> pair(fake_id, pt);
	prod_thrds.insert(pair);
	pthread_mutex_unlock(&map_lock);
}

int Factory::stealProducts(int num_products,unsigned int fake_id){
	thief_count++;
	
	pthread_mutex_lock(&avail_lock);
	while(door_open == false){
		pthread_mutex_unlock(&avail_lock);
		pthread_mutex_lock(&avail_lock);
	}
	pthread_mutex_unlock(&avail_lock);
	
	pthread_cond_wait(&thief_in, &avail_lock);
	
	pthread_mutex_lock(&avail_lock);
	while(!avail.empty() && num_products){
		Product p = avail.back();
		avail.pop_back();
		const std::pair<Product, unsigned int> pair(p, fake_id);
		stolen.push_front(pair);
		num_products--;
		
	}
	thief_count--;
	pthread_mutex_unlock(&avail_lock);
	
	//pthread_mutex_lock(&map_lock);
	if(!thief_count){ 
		pthread_cond_broadcast(&company_in);
	} else {
		pthread_cond_broadcast(&thief_in);
	}
	//pthread_mutex_unlock(&map_lock);
	
    return 0;
}

int Factory::finishThief(unsigned int fake_id){
    std::map<unsigned int,pthread_t>::iterator it;
	pthread_mutex_lock(&map_lock);
	it = prod_thrds.find(fake_id);
	//if (it != mymap.end())
	pthread_t pt = (*it).second; 
	pthread_join(pt, NULL);
	pthread_cancel(pt);
    prod_thrds.erase(it);
	pthread_mutex_unlock(&map_lock);
	
	return fake_id;
}

void Factory::closeFactory(){
	pthread_mutex_lock(&avail_lock);
	door_open = false;
	//pthread_cond_broadcast(&thief_in);
	pthread_mutex_unlock(&avail_lock);
}

void Factory::openFactory(){
	pthread_mutex_lock(&avail_lock);
	door_open = true;
	//pthread_cond_broadcast(&thief_in);
	pthread_mutex_unlock(&avail_lock);
}

void Factory::closeReturningService(){
	pthread_mutex_lock(&avail_lock);
	returning_service_open = false;
	//pthread_cond_broadcast(&thief_in);
	pthread_mutex_unlock(&avail_lock);
}

void Factory::openReturningService(){
	pthread_mutex_lock(&avail_lock);
	returning_service_open = true;
	//pthread_cond_broadcast(&thief_in);
	pthread_mutex_unlock(&avail_lock);
}

std::list<std::pair<Product, int> > Factory::listStolenProducts(){
	pthread_mutex_lock(&avail_lock);
    return stolen;
	pthread_mutex_unlock(&avail_lock);
}

std::list<Product> Factory::listAvailableProducts(){
	pthread_mutex_lock(&avail_lock);
    return avail;
	pthread_mutex_unlock(&avail_lock);
}

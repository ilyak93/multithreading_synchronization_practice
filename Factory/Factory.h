#ifndef FACTORY_H_
#define FACTORY_H_

#include <pthread.h>
#include <list>
#include <map>
#include "Product.h"

class Factory{
public:
    Factory();
    ~Factory();
    
    void startProduction(int num_products, Product* products, unsigned int id);
    void produce(int num_products, Product* products);
    void finishProduction(unsigned int id);
    
    void startSimpleBuyer(unsigned int id);
    int tryBuyOne();
    int finishSimpleBuyer(unsigned int id);

    void startCompanyBuyer(int num_products, int min_value,unsigned int id);
    std::list<Product> buyProducts(int num_products);
    void returnProducts(std::list<Product> products,unsigned int id);
    int finishCompanyBuyer(unsigned int id);

    void startThief(int num_products,unsigned int fake_id);
    int stealProducts(int num_products,unsigned int fake_id);
    int finishThief(unsigned int fake_id);

    void closeFactory();
    void openFactory();
    
    void closeReturningService();
    void openReturningService();
    
    std::list<std::pair<Product, int> > listStolenProducts();
    std::list<Product> listAvailableProducts();
	
protected:
	pthread_mutex_t avail_lock;
	pthread_mutex_t map_lock;
	std::map<unsigned int, pthread_t> prod_thrds;
	std::list<Product> avail;
	std::list<std::pair<Product, int> > stolen;
	int thief_count;
	pthread_cond_t thief_in;
	pthread_cond_t company_in;
	bool door_open;
	bool returning_service_open;
	
};
#endif // FACTORY_H_

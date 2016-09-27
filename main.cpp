#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <vector>

using namespace std;

#define NUM_THREADS 5

vector<long> vector;

void printVector()
{
    for(int i = 0; i < vector.size(); ++i)
    {
	cout << vector[i] << "\t" <<endl;
    }
}

void push(void* threadid)
{
	long tid;
	tid = (long)threadid;
	vec.push_back(tid);
	cout << "pushing " << tid  <<endl;
}

void pop(void* threadid)
{
	if (vec.size() > 0)
	{
		long val = vec.back();
		vec.pop_back(val);
		cout << "Popping "<< val << endl;
	}
}

int main()
{

    pthread_t threads[NUM_THREADS];
    int rc;
    for(int i = 0; i < NUM_THREADS; ++i)
    {
	if((i % 2) == 0)
	{
	    rc = pthread_create(&threads[i],NULL,push, (void*)i);
	}
	else
	{
	    rc = pthread_create(&threads[i],NULL,pop, (void*)i);
	}
	if(rc)
	{
	    cout << "Error: imposible crear el hilo" <<endl;
	    exit(-1);
	}
    }
    pthread_exit(NULL);
    printVector();	

}


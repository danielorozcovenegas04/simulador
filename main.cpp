#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <vector>

using namespace std;

#define NUM_THREADS 5

vector<long> vec;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* push(void* threadid)
{
	long tid;
	tid = (long)threadid;
	pthread_mutex_lock(&mutex);
	    vec.push_back(tid);
	    cout << "pushing " << tid <<endl;
	pthread_mutex_unlock(&mutex);
}

void* pop(void* threadid)
{
	if (!vec.empty())
	{
		pthread_mutex_lock(&mutex);
		    long val = vec.back();
		    long tid = (long)threadid;
		    vec.pop_back();
		pthread_mutex_unlock(&mutex);
		cout << "popping "<< val << " by " << tid << endl;
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

}


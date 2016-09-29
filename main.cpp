#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <vector>

using namespace std;

#define NUM_THREADS 5

vector<long> vec;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrera;

void* push(void* threadid)
{
	long tid;
	tid = (long)threadid;
	pthread_mutex_lock(&mutex);
	    vec.push_back(tid);
	    cout << "pushing " << tid <<endl;
	pthread_mutex_unlock(&mutex);
	pthread_barrier_wait(&barrera);
	pop(threadid);
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
    pthread_barrier_init(&barrera,NULL,NUM_THREADS);
    for(int i = 0; i < NUM_THREADS; ++i)
    {
		rc = pthread_create(&threads[i],NULL,push, (void*)i);
		if(rc)
		{
	    	cout << "Error: imposible crear el hilo" <<endl;
	    	exit(-1);
		}
    }
    pthread_exit(NULL);
    pthread_barrier_destroy(&barrera);

}


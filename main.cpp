#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <vector>

using namespace std;

#define NUM_THREADS 5

vector<int> vector;

void printVector()
{
    for(int i = 0; i < vector.size(); ++i)
    {
	cout << vector[i] << "\t" <<endl;
    }
}

void push(void* threadid)
{
	int tid;
	tid = (int)threadid;
	vec.push_back(tid);
	cout << "pushing " << tid  <<endl;
}

void pop(void* threadid)
{
	if (vec.size() > 0)
	{
		int val = vec.back();
		int tid = (int)threadid;
		vec.pop_back();
		cout << "Popping "<< val << "by " << tid << endl;
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


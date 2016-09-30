#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <vector>
#include "Procesador.h"

using namespace std;

#define NUM_THREADS 3

vector<long> vec;		//vector de prueba

int memPDatos[96];		//vector que representa la memoria principal compartida de datos, son 96 celdas pues son 24 bloques 
						//y a cada bloque le caben 4 datos, aunque se simula que cada dato son 4 bytes, al usar enteros 
						//solo se necesita un entero para cada dato
						
int memPInst[640];		//vector que representa la memoria principal compartida de instrucciones, son 640 celdas porque son
						//40 bloques y a cada bloque le caben 4 palabras, una palabra es una instruccion, y cada palabra tiene 4 enteros
						
int matrizHilillos[8][37];		//representa los contextos de todos los hilillos y además la cantidad de ciclos de procesamiento 
								//y su estado ("sin terminar" = 0, "terminado" = 1), asi como el tiempo real que duro el hilillo en terminar
								
int* colaHilillos;		//representa el proximo hilillo en espera de ser cargado a algun procesador

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrera1;
pthread_barrier_t barrera2;

void* pop(void*);

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

void sacarContexto()
{
	
}

void cargarContexto()
{
	
}

int main()
{

    pthread_t threads[NUM_THREADS];
    int rc;
    pthread_barrier_init(&barrera1,NULL,NUM_THREADS);
    	//operaciones entre cada cambio de ciclo de reloj
    pthread_barrier_init(&barrera2,NULL,NUM_THREADS);
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
    pthread_barrier_destroy(&barrera1);
    pthread_barrier_destroy(&barrera2);

}


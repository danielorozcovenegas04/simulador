#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <vector>
#include "Procesador.h"
#include <fstream>

using namespace std;

#define NUM_THREADS 3
#define NUM_HILILLOS 5	//numero de hilillos a ejecutar

vector<long> vec;		//vector de prueba

int memPDatos[96];		//vector que representa la memoria principal compartida de datos, son 96 celdas pues son 24 bloques 
						//y a cada bloque le caben 4 datos, aunque se simula que cada dato son 4 bytes, al usar enteros 
						//solo se necesita un entero para cada dato
						
int memPInst[640];		//vector que representa la memoria principal compartida de instrucciones, son 640 celdas porque son
						//40 bloques y a cada bloque le caben 4 palabras, una palabra es una instruccion, y cada palabra tiene 4 enteros
						
int matrizHilillos[NUM_HILILLOS][37];		//representa los contextos de todos los hilillos y además la cantidad de ciclos de procesamiento 
								//y su estado ("sin comenzar" = 0, "en espera" = 1,"terminado" = 2), asi como el tiempo real que duro el hilillo 
								//en terminar
								
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
	cout << "Esperando en barrera1\n" <<endl;
	pthread_barrier_wait(&barrera1);
	pop(threadid);
	cout << "Esperando en barrera2\n" <<endl;
	pthread_barrier_wait(&barrera2);
	pthread_exit(NULL);
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

//determinar bloque en memoria principal de instrucciones según PC
int bloqEnMPInstr(int pPC)
{
	int numBloque;
	//verifica que la dirección de memoria sea valida para la memoria principal compartida de instrucciones
	if(pPC >= 384)
	{
		numBloque = pPC / 16; //numero de bloque = (dirección de memoria / cantidad de Bytes por bloque)
	}
	return numBloque;
}

//determinar palabra en bloque de memoria principal de instrucciones según PC
int palEnBloqEnMPInstr(int pPC)
{
	int numPalabra;
	//verifica que la dirección de memoria sea valida para la memoria principal compartida de instrucciones
	if(pPC >= 384)
	{
		numPalabra = bloqEnMPInstr(pPC) / 4; //numero de palabra en bloque = (numero de bloque / cantidad de Bytes por palabra)
	}
	return numBloque;
}

//carga hilillos de los archivos de texto a la matriz de contextos
void cargarHilillos()
{
	int v1,v2,v3,v4;
	for(int i = 0; i < 5; ++i)
	{
		bool primerInstr = true;
		ifstream flujoEntrada(""+ i +".txt", ios::in);
		if(!flujoEntrada)
		{
			cerr << "No se pudo abrir el archivo" << endl;
			exit(1);
		}
		
		while(flujoEntrada >> v1 >> v2 >> v3 >> v4)
		{
			if(primerInstr)
			{
				matrizHilillos[i][0] = v1;	
			}
			primerInstr = false;
			
		}
	}
}

int main()
{
	
    pthread_t threads[NUM_THREADS];
    int rc;
    pthread_barrier_init(&barrera1,NULL,NUM_THREADS);
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
    for(int j = 0; j < NUM_THREADS; j++)
    {
    	pthread_join(threads[j], NULL);
    }
    pthread_barrier_destroy(&barrera1);
    pthread_barrier_destroy(&barrera2);

}


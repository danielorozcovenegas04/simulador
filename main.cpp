#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <vector>
#include "Procesador.h"
#include <fstream>
#include <time.h>

using namespace std;

#define NUM_THREADS 3
#define NUM_HILILLOS 5	//numero de hilillos a ejecutar
#define TAMA_MPInst 640 //son 640 celdas porque son 40 bloques y a cada bloque le caben 4 palabras, una palabra es una instruccion, 
						//y cada palabra tiene 4 enteros
#define TAM_MPDat 96	//son 96 celdas pues son 24 bloques y a cada bloque le caben 4 datos, aunque se simula que cada dato son 4 bytes, 
						//al usar enteros solo se necesita un entero para cada dato
						
#define QUANTUM 40;

vector<long> vec;		//vector de prueba

int memPDatos[TAM_MPDat];		//vector que representa la memoria principal compartida de datos
						
int memPInst[TAMA_MPInst];		//vector que representa la memoria principal compartida de instrucciones
						
int matrizHilillos[NUM_HILILLOS][37];		//representa los contextos de todos los hilillos y además la cantidad de ciclos de procesamiento 
								//y su estado ("sin comenzar" = 0, "en espera" = 1, "corriendo" = 2,"terminado" = 3), y un identificador de hilillo
								
time_t tiemposXHilillo[NUM_HILILLOS];		//el tiempo real que duro el hilillo en terminar, en un inicio tendra los tiempos en los que cada
											//hilillo se comenzo a ejecutar
								
int* colaHilillos;		//representa el proximo hilillo en espera de ser cargado a algun procesador

time_t tiempoInicio;			//almacenará temporalmente el tiempo en que inicio cada hilillo
time_t tiempoFin;				//almacenará temporalmente el tiempo en que termino cada hilillo

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

//carga un hilillo en un hilo para ser ejecutado
void cargarContexto(int pNumNucleo)
{
	switch(pNumNucleo)
	{
		case 0:
			procesador0.setId(colaHilillos[36]);
			procesador0.setQuantum(QUANTUM);
			procesador0.setEstadoHilillo(colaHilillos[35]);
			procesador0.setCiclos(colaHilillos[34]);
			procesador0.setRegsPC(colaHilillos);
			break;
		case 1:
			procesador1.setId(colaHilillos[36]);
			procesador1.setQuantum(QUANTUM);
			procesador1.setEstadoHilillo(colaHilillos[35]);
			procesador1.setCiclos(colaHilillos[34]);
			procesador1.setRegsPC(colaHilillos);
			break;
		case 2:
			procesador2.setId(colaHilillos[36]);
			procesador2.setQuantum(QUANTUM);
			procesador2.setEstadoHilillo(colaHilillos[35]);
			procesador2.setCiclos(colaHilillos[34]);
			procesador2.setRegsPC(colaHilillos);
			break;
	}
}

//indica si aun existen hilillos en espera de ser ejecutados
bool buscarHilillosEnEspera()
{
	bool sentinela = false; //vigila si ya se encontro un hilillo en espera
	//mientras no se haya encontrado un hilillo en espera y la celda actual pertenezca a la matriz de hilillos
	for(int i = 0; i < NUM_HILILLOS && !sentinela; ++i)
	{
		if(matrizHilillos[i][35] == 1)
		{
			sentinela = true;
		}
	}
	return sentinela;
}

void correr(int pNumNucleo)
{
	switch(pNumNucleo)
	{
		case 0:
			Procesador procesador0 = new Procesador(pNumNucleo);
			break;
		case 1:
			Procesador procesador1 = new Procesador(pNumNucleo);
			break;
		case 2:
			Procesador procesador2 = new Procesador(pNumNucleo);
			break;
	}
	pthread_barrier_wait(&barrera1);
	switch(pNumNucleo)
	{
		case 0:
			procesador0.setVecProcs(&procesador1, &procesador2);
			//el hilo correra hasta que no hayan mas hilillos en espera
			while(*colaHilillos != -1)
			{
				//inicia zona critica
				pthread_mutex_lock(&mutex);
					if(colaHilillos[35] == 0 || colaHilillos[35] == 1)
					{
						if(colaHilillos[35] == 0)
						{
							time(&tiempoInicio);
							tiemposXHilillo[colaHilillos[36]] = tiempoInicio;
						}
						colaHilillos[35] = 2;
						cargarContexto(pNumNucleo);
					}
				pthread_mutex_unlock(&mutex);
				//termina zona critica
				procesador0.correrInstruccion(procesador0.obtenerInstruccion());
			}
			break;
		case 1:
			
			break;
		case 2:
			
			break;
	}
}

int crearNucleos()
{
	pthread_t threads[NUM_THREADS];
    int rc;
    pthread_barrier_init(&barrera1,NULL,NUM_THREADS);
    pthread_barrier_init(&barrera2,NULL,NUM_THREADS);
    for(int i = 0; i < NUM_THREADS; ++i)
    {
		rc = pthread_create(&threads[i],NULL,correr, (void*)i);
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

//determinar bloque en memoria principal de instrucciones según PC
int buscarBloqEnMPInstr(int pPC)
{
	int numBloque;
	//verifica que la dirección de memoria sea valida para la memoria principal compartida de instrucciones
	if(pPC >= 384)
	{
		numBloque = pPC / 16; //numero de bloque = (dirección de memoria / cantidad de Bytes por bloque)
	}
	else
	{
		numBloque = -1;
	}
	return numBloque;
}

//determinar palabra en bloque de memoria principal de instrucciones según PC
int buscarPalEnBloqEnMPInstr(int pPC)
{
	int numPalabra;
	//verifica que la dirección de memoria sea valida para la memoria principal compartida de instrucciones
	if(pPC >= 384)
	{
		numPalabra = (pPC % 16) / 4; //numero de palabra en bloque = ((dirección de memoria % cantidad de Bytes por bloque) / cantidad de Bytes por palabra)
	}
	else
	{
		numPalabra = -1;
	}
	return numPalabra;
}

//carga hilillos de los archivos de texto a la matriz de contextos
void cargarHilillos()
{
	int v1,v2,v3,v4;
	int direccion;		//indica la proxima dirección en donde guardar una instrucción
	int contadorInstrucciones;	//indica la cantidad de instrucciones ingresadas en memoria actualmente
	for(int i = 0; i < NUM_HILILLOS; ++i)
	{
		bool primerInstr = true;
		int PC;
		ifstream flujoEntrada1("0.txt", ios::in);
		ifstream flujoEntrada2("1.txt", ios::in);
		ifstream flujoEntrada3("2.txt", ios::in);
		ifstream flujoEntrada4("3.txt", ios::in);
		ifstream flujoEntrada5("4.txt", ios::in);
		if(!flujoEntrada1 || !flujoEntrada2 || !flujoEntrada3 || !flujoEntrada4 || !flujoEntrada5)
		{
			cerr << "No se pudo abrir un archivo" << endl;
			exit(1);
		}
		
		if(i == 0)
		{
			while(flujoEntrada1 >> v1 >> v2 >> v3 >> v4)
			{
				PC = 384 + (4 * contadorInstrucciones);
				if(primerInstr)
				{
					matrizHilillos[i][0] = 	PC;
					matrizHilillos[i][35] = 0;
					colaHilillos = &(matrizHilillos[0]);
					primerInstr = false;
				}
				memPInst[PC - 384] = v1;
				memPInst[(PC + 1) - 384] = v2;
				memPInst[(PC + 2) - 384] = v3;
				memPInst[(PC + 3) - 384] = v4;
				contadorInstrucciones++;
			}
		}
		else
		{
			if(i == 1)
			{
				while(flujoEntrada2 >> v1 >> v2 >> v3 >> v4)
				{
					PC = 384 + (4 * contadorInstrucciones);
					if(primerInstr)
					{
						matrizHilillos[i][0] = 	PC;
						matrizHilillos[i][35] = 0;
						primerInstr = false;
					}
					memPInst[PC - 384] = v1;
					memPInst[(PC + 1) - 384] = v2;
					memPInst[(PC + 2) - 384] = v3;
					memPInst[(PC + 3) - 384] = v4;
					contadorInstrucciones++;
				}
			}
			else
			{
				if(i == 2)
				{
					while(flujoEntrada3 >> v1 >> v2 >> v3 >> v4)
					{
						PC = 384 + (4 * contadorInstrucciones);
						if(primerInstr)
						{
							matrizHilillos[i][0] = 	PC;
							matrizHilillos[i][35] = 0;
							primerInstr = false;
						}
						memPInst[PC - 384] = v1;
						memPInst[(PC + 1) - 384] = v2;
						memPInst[(PC + 2) - 384] = v3;
						memPInst[(PC + 3) - 384] = v4;
						contadorInstrucciones++;
					}
				}
				else
				{
					if(i == 3)
					{
						while(flujoEntrada4 >> v1 >> v2 >> v3 >> v4)
						{
							PC = 384 + (4 * contadorInstrucciones);
							if(primerInstr)
							{
								matrizHilillos[i][0] = 	PC;
								matrizHilillos[i][35] = 0;
								primerInstr = false;
							}
							memPInst[PC - 384] = v1;
							memPInst[(PC + 1) - 384] = v2;
							memPInst[(PC + 2) - 384] = v3;
							memPInst[(PC + 3) - 384] = v4;
							contadorInstrucciones++;
						}
					}
					else
					{
						if(i == 4)
						{
							while(flujoEntrada5 >> v1 >> v2 >> v3 >> v4)
							{
								PC = 384 + (4 * contadorInstrucciones);
								if(primerInstr)
								{
									matrizHilillos[i][0] = 	PC;
									matrizHilillos[i][35] = 0;
									primerInstr = false;
								}
								memPInst[PC - 384] = v1;
								memPInst[(PC + 1) - 384] = v2;
								memPInst[(PC + 2) - 384] = v3;
								memPInst[(PC + 3) - 384] = v4;
								contadorInstrucciones++;
							}
						}
					}
				}
			}
		}
	}
}

void imprimirMPInstr()
{
	for(int z = 0; z < TAMA_MPInst; z += 4)
	{
		cout << memPInst[z] << "\t" << memPInst[z + 1] << "\t" << memPInst[z + 2] << "\t" << memPInst[z + 3] << "\t" << endl;
	}
}

void imprimirMatrizHilillos()
{
	for(int x = 0; x < NUM_HILILLOS; ++x)
	{
		for(int y = 0; y < 37; ++y)
		{
			cout << matrizHilillos[x][y] << "  ";
		}
		/*
		if((x % 37) == 0)
		{
			cout <<endl;
		}
		*/
	}
}

int main()
{
	
	cargarHilillos();
	//imprimirMPInstr();
	//imprimirMatrizHilillos();

}


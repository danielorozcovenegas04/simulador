#include <iostream>
#include <cstdlib>
#include <pthreadh>
#include <fstream>
#include <timeh>

using namespace std;

#define NUM_THREADS 3
#define NUM_HILILLOS 5	//numero de hilillos a ejecutar
#define TAMA_MPInst 640 //son 640 celdas porque son 40 bloques y a cada bloque le caben 4 palabras, una palabra es una instruccion, 
						//y cada palabra tiene 4 enteros
#define TAM_MPDat 96	//son 96 celdas pues son 24 bloques y a cada bloque le caben 4 datos, aunque se simula que cada dato son 4 bytes, 
						//al usar enteros solo se necesita un entero para cada dato
						
#define QUANTUM 40;

int memPDatos[TAM_MPDat];		//vector que representa la memoria principal compartida de datos
						
int memPInst[TAMA_MPInst];		//vector que representa la memoria principal compartida de instrucciones
						
int matrizHilillos[NUM_HILILLOS][37];		//representa los contextos de todos los hilillos y además la cantidad de ciclos de procesamiento 
								//y su estado ("sin comenzar" = 0, "en espera" = 1, "corriendo" = 2,"terminado" = 3), y un identificador de hilillo
								
time_t tiemposXHilillo[NUM_HILILLOS];		//el tiempo real que duro el hilillo en terminar, en un inicio tendra los tiempos en los que cada
											//hilillo se comenzo a ejecutar
								
int* colaHilillos;		//representa el proximo hilillo en espera de ser cargado a algun procesador

time_t tiempoInicio;			//almacenará temporalmente el tiempo en que inicio cada hilillo
time_t tiempoFin;				//almacenará temporalmente el tiempo en que termino cada hilillo

class Procesador;

Procesador vecProcs[NUM_THREADS];	//vector de procesadores o hilos
pthread_t threads[NUM_THREADS];		//identificadores de los hilos

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bus = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrera1;
pthread_barrier_t barrera2;


//inicio clase Procesador
class Procesador
{
	private:
		long id;                    //identificador del nucleo
	    int regsPC[34];             //vector con los 32 registros generales, el RL y el PC
	    int cacheInst[6][4][4];     //matriz que representa la cache de instrucciones, tridimensional porque cada palabra es de cuatro enteros
	    int cacheDat[6][4];         //matriz que representa la cache de datos, no es tridimensional porque cada 
	                                //dato en una direccion se interpreta internamente como un solo entero
	    int estadoHilillo;          //tiene un 0 si no ha comenzado su ejecucion, un 1 si esta en espera, un 2 si esta corriendo, y un 3 si ya termino
	    int ciclosUsados;	
		pthread_mutex_t mutexCacheInstLocal;
		pthread_mutex_t mutexCacheDatLocal;
		int quantum;
	
	public:
		Procesador(int pID)
		{
		    id = pID;
		}
		
		Procesador()
		{
		    id = 0;
		}
		
		~Procesador()
		{
		    
		}
		
		void setId(long p_Id)
		{
		    id = p_Id;
		}
		
		long getId()
		{
		    return id;
		}
		
		void setQuantum(int p_quantum)
		{
		    quantum = p_quantum;
		}
		
		int getQuantum()
		{
		    return quantum;
		}   
		
		void setCiclos(int p_ciclos)
		{
		    ciclosUsados = p_ciclos;
		}
		
		int getCiclos()
		{
		    return ciclosUsados;
		}
		
		void setCacheInst(int*** p_cacheInst)
		{
		    cacheInst = p_cacheInst;
		}
		
		int*** getCacheInst()
		{
		    return cacheInst;
		}
		
		void setCacheDat(int** p_cacheDat)
		{
		    cacheDat = p_cacheDat;
		}
		
		int** getCacheDat()
		{
		    return cacheDat;
		}
		
		void setEstadoHilillo(int p_estado)
		{
		    estadoHilillo = p_estado;
		}
		
		int getEstadoHilillo()
		{
		    return estadoHilillo;
		}
		
		pthread_mutex_t* getMutexCacheInst()
		{
			return &mutexCacheInstLocal;
		}
		
		pthread_mutex_t* getMutexCacheDat()
		{
			return &mutexCacheDatLocal;
		}
		
		void resolverFalloDeCacheInstr()
		{
		    int numBloqueEnMP = (regsPC[0] / 16);
		    int numBloqueEnCache = (numBloqueEnMP %  4);
		    int direccionEnArregloMPInstr = (numBloqueEnMP * 16) - 384;
		    bool sentinela = false;
		    while(!sentinela)
		    {
			    if(pthread_mutex_trylock(&mutexCacheInstLocal) == 0)
			    {
					//si tengo el bus
					if(pthread_mutex_trylock(&bus) == 0)
					{
						for(int indice = 0; indice < NUM_THREADS; ++indice)
						{
							if(indice != id)
							{
								if(pthread_mutex_trylock((vecProcs[indice]).getMutexCacheDat()) == 0)
								{
								    //si el bloque esta en la caché
								    if((vecProcs[indice]).getCacheInst()[4][numBloqueEnCache][0] == numBloqueEnMP)
								    {
								    	((vecProcs[indice]).getCacheInst()[5][numBloqueEnCache][0] = 0;  //se invalida   						
								    }
								    pthread_mutex_unlock((vecProcs[indice]).getMutexCacheInst());		//se libera cache remota
								    for(int i = direccionEnArregloMPInstr; i < (direccionEnArregloMPInstr + 16); ++i)
								    {
								        for(int j = 0; j < 4; ++j)
								        {
					            			cacheInst[i / 4][numBloqueEnCache][j] = memPInst[i];
								        }
								    }
								    cacheInst[4][numBloqueEnCache][0] = numBloqueEnMP;
									cacheInst[5][numBloqueEnCache][0] = 1;
									//se simulan los 28 ciclos que toma resolver un fallo de cache
									for(int d = 0; d < 28; ++d)
									{ 
										pthread_barrier_wait(&barrera1);
									}
									sentinela = true;
									ciclosUsados += 28;
								}
					       		else
					       		{
					       			pthread_mutex_unlock(&bus);
					       			break;
					       		}
					       	}
				       	}
				       	pthread_mutex_unlock(&bus);
					}
					pthread_mutex_unlock(&mutexCacheInstLocal);
			    }
		    }
		}
		
		//metodo poco util
		void resolverFalloDeCacheDat(int pNumBloqEnMP)
		{
		    int numBloqueEnMP = (regsPC[0] / 16);
		    int numBloqueEnCache = (numBloqueEnMP %  4);
		    int direccionEnArregloMPDat = (numBloqueEnMP * 16);
		    bool sentinela = false;
		    while(!sentinela)
		    {
			    if(pthread_mutex_trylock(&mutexCacheDatLocal) == 0)
			    {
					//si tengo el bus
					if(pthread_mutex_trylock(bus) == 0)
					{
						for(int indice = 0; indice < NUM_THREADS; ++indice)
						{
							if(indice != id)
							{
								if(pthread_mutex_trylock((vecProcs[indice]).getMutexCacheDat()) == 0)
								{
								    //si el bloque esta en la caché
								    if((vecProcs[indice]).getCacheDat()[4][numBloqueEnCache][0] == numBloqueEnMP)
								    {
								    	(vecProcs[indice]).getCacheDat()[5][numBloqueEnCache][0] = 0;  //se invalida   						
								    }
								    pthread_mutex_unlock((vecProcs[indice]).getMutexCacheDat());		//se libera cache remota
								    for(int i = direccionEnArregloMPDat; i < (direccionEnArregloMPDat + 4); ++i)
								    {
					            		cacheDat[i][numBloqueEnCache] = memPDatos[i];
								    }
								    cacheDat[4][numBloqueEnCache] = numBloqueEnMP;
									cacheDat[5][numBloqueEnCache] = 1;
									for(int d = 0; d < 28; ++d)
									{ 
										pthread_barrier_wait(&barrera1);
									}
									sentinela = true;
									ciclosUsados += 28;
								}
					       		else
					       		{
					       			pthread_mutex_unlock(&bus);
					       			break;
					       		}
					       	}
				       	}
				       	pthread_mutex_unlock(&bus);
					}
					pthread_mutex_unlock(&mutexCacheDatLocal);
			    }
		    }
		}
		
		//busca el bloque en la cache de instrucciones y retorna el número de bloque en caché de estar valido, en caso de no estar o estar invalido retorna un -1
		int buscarBloqEnCacheInstr()
		{
		    int numBloqueEnMP = (regsPC[0] / 16);
		    int numBloqueEnCache = (numBloqueEnMP %  4);  //numero de bloque a retornar
		    //si el bloque no esta en la caché
		    if(cacheInst[4][numBloqueEnCache] != numBloqueEnMP)
		    {
		        numBloqueEnCache = -1;						
		    }
		    return numBloqueEnCache;
		}
		
		//busca el bloque en la cache de datos y retorna el número de bloque en caché de estar valido, en caso de no estar o estar invalido retorna un -1
		int buscarBloqEnCacheDat(int pDireccion)
		{
			int numBloqueEnMP = (pDireccion / 16);
		    int numBloqueEnCache = (numBloqueEnMP %  4);  //numero de bloque a retornar
		    //si el bloque no esta en la caché
		    if(cacheDat[4][numBloqueEnCache] != numBloqueEnMP)
		    {
		        numBloqueEnCache = -1;
		    }
		    return numBloqueEnCache;
		}
		
		int verificarValidezBloqCacheDat(int pNumBloqEnCache)
		{
			int retorno = 0;
			if(cacheDat[5][pNumBloqEnCache] == 1)
			{
				retorno = 1;
			}
			return retorno;
		}
		
		int verificarValidezBloqCacheInst(int pNumBloqEnCache)
		{
			int retorno = 0;
			if(cacheInst[5][pNumBloqEnCache][0] == 1)
			{
				retorno = 1;
			}
			return retorno;
		}
		
		//busca la palabra en el bloque de cache de instrucciones y retorna el numero de palabra, retorna -1 en caso de error
		int buscarPalEnCacheInstr()
		{
		    int numPal = -1;
		    if(buscarBloqEnCacheInstr() != -1)
		    {
		        numPal = ((regsPC[0] % 16) /  4);
		    }
		    return numPal;
		    
		}
		
		//busca la palabra en el bloque de cache de datos y retorna el numero de palabra, retorna -1 en caso de error
		int buscarPalEnCacheDat(int pDireccion)
		{
		    int numPal = -1;
		    if(buscarBloqEnCacheDat() != -1)
		    {
		        numPal = ((pDireccion % 16) /  4);
		    }
		    return numPal;
		}
		
		int* obtenerInstruccionDeCache()
		{
			int* instruccion;		
			int numeroBloq = buscarBloqEnCacheInstr();
			int numeroPal;
			if(numeroBloq == -1)
			{
			    *instruccion = -1;      //retornara un -1 si la instrucción no se encuentra en la cache de instrucciones
			}
			else
			{
			    numeroPal = buscarPalEnCacheInstr();
			    instruccion = &(cacheInst[numeroPal][numeroBloq]);
			}
			return instruccion;
		}
		
		int obtenerDatoDeCache(int pBloqEnMP, int pNumPal)
		{
		    int dato = -1;          //devolvera un -1 si el bloque no esta en la cache o si esta invalido
		    int numBloqEnCache = (pNumBloqEnMP % 4);
		    if((cacheDat[5][numBloqEnCache] == 1) && (cacheDat[4][numBloqEnCache] == pNumBloqEnMP))
		    {
		        dato = cacheDat[pNumPal][numBloqEnCache];
		    }
		    return dato;
		}
		
		//obtiene la instrucción que indique el PC
		int* obtenerInstruccion()
		{
			int instruccion[4];		//contendra la instrucción que se necesita
			instruccion = obtenerInstruccionDeCache();
			while(*instruccion == -1)
			{
			    resolverFalloDeCacheInstr();
			    instruccion = obtenerInstruccionDeCache();
			}
			return instruccion;
		}
		
		//obtiene el dato
		int obtenerDato(int pDireccion)
		{
		    int numeroBloqEnMP = (pDireccion / 16);
		    int numeroPal = ((pDireccion % 16) / 4);
		    dato = obtenerDatoDeCache(numeroBloqEnMP, numeroPal);
		    while(dato == -1)
			{
			    resolverFalloDeCacheDat(numeroBloqEnMP);
			    dato = obtenerDatoDeCache(numeroBloqEnMP, numeroPal);
			}
			return dato;
		}
		
		/**
		* Este metodo se encarga de comprobar que instrucción se va a correr, y
		* llamar al metodo correspondiente
		*/
		void correrInstruccion() 
		{
			int* palabra = obtenerInstruccion();
		    int v1 = palabra[0];
		    int v2 = palabra[1];
		    int v3 = palabra[2];
		    int v4 = palabra[3];
		
		    switch (v1) 
		    {
		        case 8:
		            DADDI(v2, v3, v4);
		            break;
		        case 32:
		            DADD(v2, v3, v4);
		            break;
		        case 34:
		            DSUB(v2, v3, v4);
		            break;
		        case 12:
		            DMUL(v2, v3, v4);
		            break;
		        case 14:
		            DDIV(v2, v3, v4);
		            break;
		        case 4:
		            BEQZ(v2, v4);
		            break;
		        case 5:
		            BNEZ(v2, v4);
		            break;
		        case 3:
		            JAL(v4);
		            break;
		        case 2:
		            JR(v2);
		            break;
		        case 35:
		            LW(v2, v3, v4);
		            break;
		        case 43:
		        	SW(v2, v3, v4);
		        	break;
		        case 63:
		            FIN();
		            break;
		    }
		}
		
		bool esRegistroValido(int RX) 
		{
		    bool validez = false;
		    if (RX >= 0 && RX < 32) {
		        validez = true;
		    }
		    return validez;
		}
		
		bool esRegDestinoValido(int RX)
		{
		    bool validez = false;
		    if((RX > 0) && (RX < 32))
		    {
		        validez = true;
		    }
		    return validez;
		}
		
		/**Se encarga de sumar el valor del registro RY con un inmediato n, y 
		* guardarlo en el registro RX
		*/
		void DADDI(int RY, int RX, int n)
		{
		    //Si el registro RX es el destino, o si uno de los registros no es
		    //valido hay error
		    if(esRegDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		        regsPC[RX + 1] = regsPC[RY + 1] + n;    //Realiza el DADDI
		        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        ciclosUsados++;
		    	quantum--;
		    }
		}
		    
		/**Se encarga de sumar el valor del registro RY con el valor el registro
		* RZ, y guardarlo en el registro RX
		*/
		void DADD(int RY, int RZ, int RX)
		{
		    //Si el registro RX es el destino, o si uno de los registros no es
		    //valido hay error
		    if(esDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		        regsPC[RX + 1] = regsPC[RY + 1] + regsPC[RZ + 1];    //Realiza el DADDI
		        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        ciclosUsados++;
		    	quantum--;
		    }
		}
		    
		/*
		* Se encarga de restarle al registro RY el valor del registro RZ y 
		* almacenarlo en RX
		*/
		void DSUB(int RY, int RZ, int RX)
		{
		    //Si el registro RX es el destino, o si uno de los registros no es
		    //valido hay error
		    if(esDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		        regsPC[RX + 1] = regsPC[RY + 1] - regsPC[RZ + 1];    //Realiza el DADDI
		        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        ciclosUsados++;
		    	quantum--;
		    }
		}
		    
		/*
		* Se encarga de multiplicar los registros RY y RZ y guardar el producto en RZ
		*/
		void DMUL(int RY, int RZ, int RX)
		{
		    //Si el registro RX es el destino, o si uno de los registros no es
		    //valido hay error
		    if(esDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		        regsPC[RX + 1] = regsPC[RY + 1] * regsPC[RZ + 1];    //Realiza el DADDI
		        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        ciclosUsados++;
		    	quantum--;
		    }
		}
		    
		/*
		* Se encarga de dividir los registros RY y RZ y guardar el producto en RX
		*/
		void DDIV(int RY, int RZ, int RX)
		{
		    //Si el registro RX es el destino, o si uno de los registros no es
		    //valido hay error
		    if(esDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		        regsPC[RX + 1] = regsPC[RY + 1] / regsPC[RZ + 1];    //Realiza el DADDI
		        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        ciclosUsados++;
		    	quantum--;
		    }
		}
		
		/*
		* Si el valor en el registro RX es un cero, entonces el PC se mueve n instrucciones
		*/
		void BEQZ(int RX, int n) 
		{
		    if(esRegistroValido(RX))
		    {
		        regsPC[0] += 4;
		        if(regsPC[RX + 1] == 0){
		            regsPC[0] = regsPC[0] + 4*n;
		        }
		        ciclosUsados++;
		    	quantum--;
		    }
		}
		
		/*
		* Si el valor en el registro RX es distinto de cero, entonces el PC se mueve n instrucciones
		*/
		void BNEZ(int RX, int n) 
		{
		    if(esRegistroValido(RX))
		    {
		        regsPC[0] += 4;
		        if(regsPC[RX + 1] != 0){
		            regsPC[0] = regsPC[0] + 4*n;
		        }
		        ciclosUsados++;
		    	quantum--;
		    }
		}
		
		/*
		* Carga un dato de cache de datos al registro indicado
		*/
		int LW(int RY, int RX, int n)
		{
			int retorno = 0;
			int direccion;
			int numBloqCache;
			int numBloqMP;
			int numPal;
			int direcBloqMP;
			bool falloCache = false;
			bool sentinela = false;
			if(esRegistroValido(RY))
			{
				direccion = n + regsPC[RY + 1];
			}
			while(!sentinela)
			{
				pthread_mutex_lock(&mutexCacheDatLocal);
				numBloqCache = buscarBloqEnCacheDat(direccion);
				if(numBloqCache != -1)
				{
					if(verificarValidezBloqCacheDat(numBloqCache) == 1)
					{
						numPal = buscarPalEnCacheDat(direccion);
						regsPC[RX + 1] = cacheDat[numPal][numBloqCache];
					}
					else
					{
						falloCache = true;
					}
				}
				else
				{
					falloCache = true;
				}
				if(falloCache)
				{
					if(pthread_mutex_trylock(&bus) == 0)
					{
						//sube el bloque de memoria principal de datos a cache de datos
						numBloquMP = (direccion / 16);
						direcBloqMP = (numBloquMP * 16);
						for(int i = direcBloqMP; i < (direcBloqMP + 16); i += 4)
						{
							cacheDat[((direcBloqMP) % 16) / 4][numBloqCache] = memPDatos[i / 4];
						}
						for(int z = 0; z < 28; ++z)
						{
							pthread_barrier_wait(&barrera1);
						}
						pthread_mutex_unlock(&bus);
						numPal = buscarPalEnCacheDat(direccion);
						if(esRegDestinoValido(RX))
						{
							//carga el dato de la cache de datos al registro indicado
							regsPC[RX + 1] = cacheDat[numPal][numBloqCache];
							retorno = 1;
							regsPC[0] += 4;
						}
						sentinela = true;
					}
					else
					{
						ciclosUsados++;
						pthread_mutex_unlock(&mutexCacheDatLocal);
					}
				}
				else
				{
					pthread_mutex_unlock(&mutexCacheDatLocal);
				}
			}
			ciclosUsados++;
			//si se ejecuto el LW exitosamente
			if(retorno == 1)
			{
				quantum--;
			}
			return retorno;
		}
		
		/*
		* carga un dato del registro indicado a memoria en la dirección indicada
		*/
		int SW(int RY, int RX, int n)
		{
			int retorno = 0;
			int direccion;
			int numBloqCache;
			int numBloqMP;
			int numPal;
			int numPalMP;
			int direcBloqMP;
			bool falloCache = false;
			bool sentinela = false;
			if(esRegistroValido(RY))
			{
				direccion = n + regsPC[RY + 1];
			}
			while(!sentinela)
			{
				pthread_mutex_lock(&mutexCacheDatLocal);
				numBloqCache = buscarBloqEnCacheDat(direccion);
				if(numBloqCache != -1)
				{
					numPal = buscarPalEnCacheDat(direccion);
				}
				if(pthread_mutex_trylock(&bus) == 0)
				{
					//pedir caches remotas
					for(int g = 0; g < NUM_THREADS; ++g)
					{
						if(((long)g) != id)
						{
							if(pthread_mutex_trylock(vecProcs[g]getMutexCacheDat()) == 0)
							{
								numBloqCache = vecProcs[g]buscarBloqEnCacheDat(direccion);
								//si bloque se encuentra en cache
								if(numBloqCache != -1)
								{
									(vecProcs[g])getCacheDat()[5][numBloqCache] = 0;
									//liberar cache remota
									pthread_mutex_unlock((vecProcs[g])getMutexCacheDat());
									ciclosUsados++;
								}
								else
								{
									//liberar cache remota
									pthread_mutex_unlock(vecProcs[g]getMutexCacheDat());
								}
							}
							else
							{
								pthread_mutex_unlock(bus);
								ciclosUsados++;
								pthread_mutex_unlock(&mutexCacheDatLocal);
								ciclosUsados++;
							}
						}
						//si se revisaron  e invalidaron las otras caches, entonces no se sigue intentando
						if(g == 2)
						{
							sentinela = true;
							//copiar dato en registro a palabra en cache de datos
							cacheDat[numPal][numBloqCache] = vecProcs[RX + 1];
							numPalMP = (direccion / 4);
							//copiar palabra de cache de datos a memoria principal de datos
							memPDatos[numPalMP] = cacheDat[numPal][numBloqCache];
							retorno = 1;
							regsPC[0] += 4;
							for(int y = 0; y < 7; ++y)
							{
								pthread_barrier_wait(&barrera1);
							}
							ciclosUsados += 7;
							pthread_mutex_unlock(&bus);
							ciclosUsados++;
							quantum--;
						}
					}
				}
				else
				{
					pthread_mutex_unlock(&mutexCacheDatLocal);
					ciclosUsados++;
				}
			}
		}
		
		/*
		* Guarda el PC actual y luego lo mueve n direcciones
		*/
		void JAL(int n) 
		{
		    regsPC[0] += 4;
		    regsPC[31 + 1] = regsPC[0];
		    regsPC[0] += n;
		    ciclosUsados++;
		    quantum--;
		}
		
		/*
		* El PC adquiere el valor presente en el registro RX
		*/
		void JR(int RX) 
		{
		    if(esRegistroValido(RX))
		    {
		        regsPC[0] = regsPC[RX + 1];
		        ciclosUsados++;
		        quantum--;
		    }
		}
		
		/*
		* Pone el estado del hilillo actual como "terminado" en la matriz de hilillos
		*/
		void FIN() 
		{
		    regsPC[0] += 4;
		    estadoHilillo = 3;
		    ciclosUsados++;
		    quantum--;
		}
	
};
//fin clase Procesador

void sacarContexto()
{
	int* regsPCTemp;
	switch(numNucleo)
	{
		case 0:
			regsPCTemp = procesador0getRegsPC();
			for(int j = 0; j < NUM_HILILLOS; ++j)
			{
				if(procesador0.getId() == matrizHilillos[j][36])
				{
					matrizHilillos[j][34] += procesador0.getCiclos();
					if(procesador0.getEstadoHilillo() != 3)
					{
						matrizHilillos[j][35] = 1;
					}
					else
					{
						matrizHilillos[j][35] = 3;
					}
					for(int f = 0; f < 34; ++f)
					{
						matrizHilillos[j][f] = regsPCTemp[f];
					}
				}
			}
			break;
		case 1:
			regsPCTemp = procesador1.getRegsPC();
			for(int j = 0; j < NUM_HILILLOS; ++j)
			{
				if(procesador1.getId() == matrizHilillos[j][36])
				{
					matrizHilillos[j][34] += procesador1.getCiclos();
					if(procesador1.getEstadoHilillo() != 3)
					{
						matrizHilillos[j][35] = 1;
					}
					else
					{
						matrizHilillos[j][35] = 3;
					}
					for(int f = 0; f < 34; ++f)
					{
						matrizHilillos[j][f] = regsPCTemp[f];
					}
				}
			}
			break;
		case 2:
			regsPCTemp = procesador2.getRegsPC();
			for(int j = 0; j < NUM_HILILLOS; ++j)
			{
				if(procesador2.getId() == matrizHilillos[j][36])
				{
					matrizHilillos[j][34] += procesador2.getCiclos();
					if(procesador2.getEstadoHilillo() != 3)
					{
						matrizHilillos[j][35] = 1;
					}
					else
					{
						matrizHilillos[j][35] = 3;
					}
					for(int f = 0; f < 34; ++f)
					{
						matrizHilillos[j][f] = regsPCTemp[f];
					}
				}
			}
			break;
	}
}

//carga un hilillo en un hilo para ser ejecutado
void cargarContexto(int numNucleo)
{
	switch(numNucleo)
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

void correr()
{
	int indiceHililloActual;
	int it;
	bool proximoHilo = false;
	//encontrar el numero de procesador actual
	int numNucleo;
	for(int i = 0; i < NUM_THREADS; ++i)
	{
		if( 0 != pthread_equal(pthread_self(), threads[i]))
		{
			numNucleo = i;
			break;
		}
	}
	Procesador procesador0;
	Procesador procesador1;
	Procesador procesador2;
	
	switch(numNucleo)
	{
		case 0:
			procesador0 = new Procesador(numNucleo);
			pthread_mutex_lock(&mutex);
				vecProcs[numNucleo] = &procesador0;
			pthread_mutex_unlock(&mutex);
			break;
		case 1:
			procesador1 = new Procesador(numNucleo);
			pthread_mutex_lock(&mutex);
				vecProcs[numNucleo] = &procesador1;
			pthread_mutex_unlock(&mutex);
			break;
		case 2:
			procesador2 = new Procesador(numNucleo);
			pthread_mutex_lock(&mutex);
				vecProcs[numNucleo] = &procesador2;
			pthread_mutex_unlock(&mutex);
			break;
	}
	
	pthread_barrier_wait(&barrera2);
	switch(numNucleo)
	{
		case 0:
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
						cargarContexto(numNucleo);
						indiceHililloActual = colaHilillos[36];
						it = indiceHililloActual + 1;
						while((it != indiceHililloActual) && !proximoHilo)
						{
							if(it >= NUM_HILILLOS)
							{
								it = 0;
							}
							if((matrizHilillos[it][35] == 0) || (matrizHilillos[it][35] == 1))
							{
								colaHilillos = &(matrizHilillos[it]);
								proximoHilo = true;
							}
							else
							{
								++it;
							}
						}
						if((it == indiceHililloActual) && !proximoHilo)
						{
							*colaHilillos = -1;
						}
					}
				pthread_mutex_unlock(&mutex);
				//termina zona critica
				while((procesador0.getQuantum() > 0) && (procesador0.getEstadoHilillo() != 3))
				{
					procesador0.correrInstruccion();
				}
				//como se acabo el quantum o se termino de ejecutar el hilillo, entonces se saca el contexto
				pthread_barrier_wait(&barrera2);
				pthread_mutex_lock(&mutex);
					sacarContexto();
				pthread_mutex_unlock(&mutex);
			}
			break;
		case 1:
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
						cargarContexto(numNucleo);
						indiceHililloActual = colaHilillos[36];
						it = indiceHililloActual + 1;
						while((it != indiceHililloActual) && !proximoHilo)
						{
							if(it >= NUM_HILILLOS)
							{
								it = 0;
							}
							if((matrizHilillos[it][35] == 0) || (matrizHilillos[it][35] == 1))
							{
								colaHilillos = &(matrizHilillos[it]);
								proximoHilo = true;
							}
							else
							{
								++it;
							}
						}
						if((it == indiceHililloActual) && !proximoHilo)
						{
							*colaHilillos = -1;
						}
					}
				pthread_mutex_unlock(&mutex);
				//termina zona critica
				while((procesador1.getQuantum() > 0) && (procesador1.getEstadoHilillo() != 3))
				{
					procesador1.correrInstruccion();
				}
				//como se acabo el quantum o se termino de ejecutar el hilillo, entonces se saca el contexto
				pthread_barrier_wait(&barrera2);
				pthread_mutex_lock(&mutex);
					sacarContexto();
				pthread_mutex_unlock(&mutex);
			}
			break;
		case 2:
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
						cargarContexto(numNucleo);
						indiceHililloActual = colaHilillos[36];
						it = indiceHililloActual + 1;
						while((it != indiceHililloActual) && !proximoHilo)
						{
							if(it >= NUM_HILILLOS)
							{
								it = 0;
							}
							if((matrizHilillos[it][35] == 0) || (matrizHilillos[it][35] == 1))
							{
								colaHilillos = &(matrizHilillos[it]);
								proximoHilo = true;
							}
							else
							{
								++it;
							}
						}
						if((it == indiceHililloActual) && !proximoHilo)
						{
							*colaHilillos = -1;
						}
					}
				pthread_mutex_unlock(&mutex);
				//termina zona critica
				while((procesador2.getQuantum() > 0) && (procesador2.getEstadoHilillo() != 3))
				{
					procesador2.correrInstruccion();
				}
				//como se acabo el quantum o se termino de ejecutar el hilillo, entonces se saca el contexto
				pthread_barrier_wait(&barrera2);
				pthread_mutex_lock(&mutex);
					sacarContexto();
				pthread_mutex_unlock(&mutex);
			}
			break;
	}
}

int crearNucleos()
{
    int rc;
    pthread_barrier_init(&barrera1,NULL,NUM_THREADS);
    pthread_barrier_init(&barrera2,NULL,NUM_THREADS);
    for(int i = 0; i < NUM_THREADS; ++i)
    {
		rc = pthread_create(&threads[i],NULL,correr, NULL);
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
		ifstream flujoEntrada1("0txt", ios::in);
		ifstream flujoEntrada2("1txt", ios::in);
		ifstream flujoEntrada3("2txt", ios::in);
		ifstream flujoEntrada4("3txt", ios::in);
		ifstream flujoEntrada5("4txt", ios::in);
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
					colaHilillos = &(matrizHilillos);
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
	crearNucleos();
	//imprimirMPInstr();
	//imprimirMatrizHilillos();

}


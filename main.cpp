#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <fstream>
#include <time.h>
#include <vector>

#define NUM_THREADS 3
#define NUM_HILILLOS 5	//numero de hilillos a ejecutar
#define TAMA_MPInst 640 //son 640 celdas porque son 40 bloques y a cada bloque le caben 4 palabras, una palabra es una instruccion,
//y cada palabra tiene 4 enteros
#define TAM_MPDat 96	//son 96 celdas pues son 24 bloques y a cada bloque le caben 4 datos, aunque se simula que cada dato son 4 bytes,
//al usar enteros solo se necesita un entero para cada dato

#define QUANTUM 40

bool hilillosTerminaron = false;	//indica true cuando se terminaron de ejecutar todos los hilillos

int memPDatos[TAM_MPDat];		//vector que representa la memoria principal compartida de datos

int memPInst[TAMA_MPInst];		//vector que representa la memoria principal compartida de instrucciones

std::vector< std::vector<int> > matrizHilillos(NUM_HILILLOS, std::vector<int>(37));		//representa los contextos de todos los hilillos y además la cantidad de ciclos de procesamiento
//y su estado ("sin comenzar" = 0, "en espera" = 1, "corriendo" = 2,"terminado" = 3), y un identificador de hilillo

time_t tiemposXHilillo[NUM_HILILLOS];		//el tiempo real que duro el hilillo en terminar, en un inicio tendra los tiempos en los que cada
//hilillo se comenzo a ejecutar

std::vector<int> colaHilillos(37);		//representa el proximo hilillo en espera de ser cargado a algun procesador

time_t tiempoInicio;			//almacenará temporalmente el tiempo en que inicio cada hilillo
time_t tiempoFin;				//almacenará temporalmente el tiempo en que termino cada hilillo

class Procesador;

std::vector<Procesador> vecProcs;  //vector de procesadores o hilos
pthread_t threads[NUM_THREADS];		//identificadores de los hilos

pthread_mutex_t mutexMatrizHilillo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCola = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexTiemposXHilillo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t busInst = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t busDat = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexVecProcs = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrera1;
pthread_barrier_t barrera2;


//inicio clase Procesador
class Procesador
{
	private:
		long id;                    //identificador del nucleo
		int idHilillo;				//identificador del hilillo actualmente cargado
	    int estadoHilillo;          //tiene un 0 si no ha comenzado su ejecucion, un 1 si esta en espera, un 2 si esta corriendo, y un 3 si ya termino
	    int ciclosUsados;	
		int quantum;
		
		struct Instruccion
		{
			int codigoOEtiqOValid;
			int rf1;
			int rd;
			int rf2Oinmediato;
		};
	
	public:
		int regsPC[34];             //vector con el PC, los 32 registros generales, y el RL
		Instruccion cacheInst[6][4];     //matriz que representa la cache de instrucciones, bidimensional (6*4) porque cada instruccion es un elemento
									   //de la matriz
		int cacheDat[6][4];         //matriz que representa la cache de datos, no es tridimensional (6*4) porque cada 
	                                //dato en una direccion se interpreta internamente como un solo entero
		pthread_mutex_t mutexCacheDatLocal = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_t mutexCacheInstLocal = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_t mutexRegsPC = PTHREAD_MUTEX_INITIALIZER;
		
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
		
		void setRegsPC(std::vector<int> &pRegsPC)
		{
			pthread_mutex_lock(&mutexRegsPC);
				for(int i = 0; i < 34; ++i)
				{
					regsPC[i] = pRegsPC[i];
				}
			pthread_mutex_unlock(&mutexRegsPC);
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
		
		void setEstadoHilillo(int p_estado)
		{
		    estadoHilillo = p_estado;
		}
		
		int getEstadoHilillo()
		{
		    return estadoHilillo;
		}
		
		//simula un ciclo de reloj
		void tick()
		{
			pthread_barrier_wait(&barrera1);	//barrera interna
			pthread_barrier_wait(&barrera2);	//barrera externa
		}
		
		//simula n ciclos de reloj
		void ticks(int pN)
		{
			for(int i = 0; i < pN; ++i)
			{
				pthread_barrier_wait(&barrera1);	//barrera interna
				pthread_barrier_wait(&barrera2);	//barrera externa
			}
		}
		
		void resolverFalloDeCacheInstr()
		{
			pthread_mutex_lock(&mutexRegsPC);
		    	int numBloqueEnMP = (regsPC[0] / 16);
		    pthread_mutex_unlock(&mutexRegsPC);
		    int numBloqueEnCache = buscarBloqEnCacheInstr();
		    int direccionEnArregloMPInstr = (numBloqueEnMP * 16) - 384;
		    bool sentinela = false;		//indica si ya se pudo cargar el bloque de MP a cache
		    //mientras que no se haya cargado el bloque a cache
		    while(!sentinela)
		    {
		    	//si se tiene cache local
			    if(pthread_mutex_trylock(&mutexCacheInstLocal) == 0)
			    {
					//si se tiene el bus
					if(pthread_mutex_trylock(&busInst) == 0)
					{
						//copia el bloque de memoria principal a cache
				       	for(int i = direccionEnArregloMPInstr; i < (direccionEnArregloMPInstr + 16); i += 4)
						{
					        cacheInst[i][numBloqueEnCache].codigoOEtiqOValid = memPInst[i];
					        cacheInst[i][numBloqueEnCache].rf1 = memPInst[i+1];
					        cacheInst[i][numBloqueEnCache].rd = memPInst[i+2];
					        cacheInst[i][numBloqueEnCache].rf2Oinmediato = memPInst[i+3];
						}
						cacheInst[4][numBloqueEnCache].codigoOEtiqOValid = numBloqueEnMP;		//se le pone etiqueta al bloque recien subido
						cacheInst[5][numBloqueEnCache].codigoOEtiqOValid = 1;					//se marca como valido
						///*
						//se simulan los 28 ciclos que toma resolver un fallo de cache
						ticks(28);
						//*/
						sentinela = true;	//ya se cargo el bloque a cache
						ciclosUsados += 28; //se registran los ciclos usados
				       	pthread_mutex_unlock(&busInst);		//se libera el bus
					}
					
					pthread_mutex_unlock(&mutexCacheInstLocal);		//se libera la cache local
			    }
		    }
		}
		
		//busca el bloque en la cache de instrucciones y retorna el número de bloque en caché de estar valido, en caso de no estar o estar invalido retorna un -1
		int buscarBloqEnCacheInstr()
		{
			pthread_mutex_lock(&mutexRegsPC);
		    	int numBloqueEnMP = (regsPC[0] / 16);
		    pthread_mutex_unlock(&mutexRegsPC);
		    int numBloqueEnCache = (numBloqueEnMP %  4);  //numero de bloque a retornar
		    //si el bloque no esta en la caché
		    if(cacheInst[4][numBloqueEnCache].codigoOEtiqOValid != numBloqueEnMP)
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
			//si el bloque es valido
			if(cacheDat[5][pNumBloqEnCache] == 1)
			{
				retorno = 1;
			}
			return retorno;
		}
		
		//busca la palabra en el bloque de cache de instrucciones y retorna el numero de palabra, retorna -1 en caso de error
		int buscarPalEnCacheInstr(int pNumBloqEnCache)
		{
		    int numPal = -1;		//retorna -1 si el bloque que contiene la palabra no esta en cache o es invalido
		    //si el bloque esta en la cache
		    if(pNumBloqEnCache != -1)
		    {
		    	pthread_mutex_lock(&mutexRegsPC);
		        	numPal = ((regsPC[0] % 16) /  4);
		        pthread_mutex_unlock(&mutexRegsPC);
		    }
		    return numPal;
		    
		}
		
		//busca la palabra en el bloque de cache de datos y retorna el numero de palabra, retorna -1 en caso de error
		int buscarPalEnCacheDat(int pDireccion, int pNumBloqEnCache)
		{
		    int numPal = -1;
		    //si el bloque esta en la cache
		    if(pNumBloqEnCache != -1)
		    {
		        numPal = ((pDireccion % 16) /  4);
		    }
		    return numPal;
		}
		
		Instruccion obtenerInstruccionDeCache()
		{
			Instruccion instruccion;	
			instruccion.codigoOEtiqOValid = -1;	//retornara un -1 en el primer miembro si la instrucción no se encuentra en la cache de instrucciones
			int numeroBloq = buscarBloqEnCacheInstr();
			int numeroPal;
			//si el bloque esta en la cache
			if(numeroBloq != -1)
			{
			    numeroPal = buscarPalEnCacheInstr(numeroBloq);
			    instruccion = cacheInst[numeroPal][numeroBloq];		//se obtiene la instruccion
			}
			return instruccion;
		}
		
		//obtiene la instrucción que indique el PC
		Instruccion obtenerInstruccion()
		{
			Instruccion instruccion = obtenerInstruccionDeCache();
			//si es fallo de cache
			while(instruccion.codigoOEtiqOValid == -1)
			{
			    resolverFalloDeCacheInstr();
			    instruccion = obtenerInstruccionDeCache();
			}
			return instruccion;
		}
		
		/**
		* Este metodo se encarga de comprobar que instrucción se va a correr, y
		* llamar al metodo correspondiente
		*/
		void correrInstruccion() 
		{
			Instruccion instruccion = obtenerInstruccion();
		    int v1 = instruccion.codigoOEtiqOValid;
		    int v2 = instruccion.rf1;
		    int v3 = instruccion.rd;
		    int v4 = instruccion.rf2Oinmediato;
		
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
		        default:
		        	std::cout << "Instruccion inexistente" <<std::endl;
		    }
		}
		
		//carga un hilillo en un hilo para ser ejecutado
		void cargarContexto()
		{
			
			idHilillo = colaHilillos[36];
			setQuantum(QUANTUM);
			setEstadoHilillo(colaHilillos[35]);
			setCiclos(colaHilillos[34]);
			setRegsPC(colaHilillos);

		}
		
		void sacarContexto()
		{
			matrizHilillos[idHilillo][34] = getCiclos();
			if(estadoHilillo != 3)
			{
				matrizHilillos[idHilillo][35] = 1;
			}
			else
			{
				matrizHilillos[idHilillo][35] = 3;
			}
			pthread_mutex_lock(&mutexRegsPC);
				for(int f = 0; f < 34; ++f)
				{
					matrizHilillos[idHilillo][f] = regsPC[f];
				}
			pthread_mutex_unlock(&mutexRegsPC);
			//pone RL en -1
			matrizHilillos[idHilillo][33] = -1;
		}
		
		void correr()
		{
			int indiceHililloActual = 0;
			int it = 0;
			bool proximoHilo = false;
			int numeroNucleo;
			int hilillosEnEspera;
			int estadoDeProximoHilillo;
			pthread_mutex_lock(&mutexCola);
				hilillosEnEspera = colaHilillos[0];
				estadoDeProximoHilillo = colaHilillos[35];
			pthread_mutex_unlock(&mutexCola);
			//el hilo correra hasta que no hayan mas hilillos en espera
			while(hilillosEnEspera != -1)
			{
				proximoHilo = false;
				//si el proximo hilillo a ejecutar aún no comienza su ejecución o esta en espera
				if(estadoDeProximoHilillo == 0 || estadoDeProximoHilillo == 1)
				{
					if(estadoDeProximoHilillo == 0)
					{
						pthread_mutex_lock(&mutexTiemposXHilillo);
							time(&tiempoInicio);
							tiemposXHilillo[idHilillo] = tiempoInicio;	//guarda el tiempo en que comenzo a ejecutarse el hilillo
						pthread_mutex_unlock(&mutexTiemposXHilillo);
					}
					pthread_mutex_lock(&mutexMatrizHilillo);
						matrizHilillos[idHilillo][35] = 2;	//se pone como "corriendo en matriz de hilillos"
					pthread_mutex_unlock(&mutexMatrizHilillo);
					pthread_mutex_lock(&mutexCola);
						colaHilillos[35] = 2;
						cargarContexto();
						indiceHililloActual = colaHilillos[36];
					pthread_mutex_unlock(&mutexCola);
					it = indiceHililloActual + 1;
					while((it != indiceHililloActual) && !proximoHilo)
					{
						if(it >= NUM_HILILLOS)
						{
							it = 0;
						}
						pthread_mutex_lock(&mutexMatrizHilillo);
						pthread_mutex_lock(&mutexCola);
						//si aún no ha comenzado a ejecutarse el hilillo indicado por el iterador o si esta en espera
						if((matrizHilillos[it][35] == 0) || (matrizHilillos[it][35] == 1))
						{
							for(int r = 0; r < 37; ++r)
							{
								colaHilillos[r] = matrizHilillos[it][r];
							}
							proximoHilo = true;
							hilillosEnEspera = colaHilillos[0];
							estadoDeProximoHilillo = colaHilillos[35];
						}
						else
						{
							++it;
						}
						pthread_mutex_unlock(&mutexCola);
						pthread_mutex_unlock(&mutexMatrizHilillo);
					}
					if((it == indiceHililloActual) && !proximoHilo)
					{
						pthread_mutex_lock(&mutexCola);
							colaHilillos[0] = -1;
							hilillosEnEspera = colaHilillos[0];
						pthread_mutex_unlock(&mutexCola);
					}
				}
				while((quantum > 0) && (estadoHilillo != 3))
				{
					correrInstruccion();
				}
				//como se acabo el quantum o se termino de ejecutar el hilillo, entonces se saca el contexto
				pthread_mutex_lock(&mutexMatrizHilillo);
					sacarContexto();
				pthread_mutex_unlock(&mutexMatrizHilillo);
				//se simula un ciclo de reloj
				tick();
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
		    	pthread_mutex_lock(&mutexRegsPC);
			        regsPC[RX + 1] = regsPC[RY + 1] + n;    //Realiza el DADDI
			        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        pthread_mutex_unlock(&mutexRegsPC);
		        ciclosUsados++;
		    	quantum--;
		    	tick();		//simula un ciclo de reloj
		    }
		}
		    
		/**Se encarga de sumar el valor del registro RY con el valor el registro
		* RZ, y guardarlo en el registro RX
		*/
		void DADD(int RY, int RZ, int RX)
		{
		    //Si el registro RX es el destino, o si uno de los registros no es
		    //valido hay error
		    if(esRegDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		    	pthread_mutex_lock(&mutexRegsPC);
			        regsPC[RX + 1] = regsPC[RY + 1] + regsPC[RZ + 1];    //Realiza el DADDI
			        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        pthread_mutex_unlock(&mutexRegsPC);
		        ciclosUsados++;
		    	quantum--;
		    	tick();		//simula un ciclo de reloj
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
		    if(esRegDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		    	pthread_mutex_lock(&mutexRegsPC);
			        regsPC[RX + 1] = regsPC[RY + 1] - regsPC[RZ + 1];    //Realiza el DADDI
			        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        pthread_mutex_unlock(&mutexRegsPC);
		        ciclosUsados++;
		    	quantum--;
		    	tick();		//simula un ciclo de reloj
		    }
		}
		    
		/*
		* Se encarga de multiplicar los registros RY y RZ y guardar el producto en RZ
		*/
		void DMUL(int RY, int RZ, int RX)
		{
		    //Si el registro RX es el destino, o si uno de los registros no es
		    //valido hay error
		    if(esRegDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		    	pthread_mutex_lock(&mutexRegsPC);
			        regsPC[RX + 1] = regsPC[RY + 1] * regsPC[RZ + 1];    //Realiza el DADDI
			        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        pthread_mutex_unlock(&mutexRegsPC);
		        ciclosUsados++;
		    	quantum--;
		    	tick();		//simula un ciclo de reloj
		    }
		}
		    
		/*
		* Se encarga de dividir los registros RY y RZ y guardar el producto en RX
		*/
		void DDIV(int RY, int RZ, int RX)
		{
		    //Si el registro RX es el destino, o si uno de los registros no es
		    //valido hay error
		    if(esRegDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
		    {
		    	pthread_mutex_lock(&mutexRegsPC);
			        regsPC[RX + 1] = regsPC[RY + 1] / regsPC[RZ + 1];    //Realiza el DADDI
			        regsPC[0] += 4;                             //Suma 4 al PC para pasar a la siguiente instruccion
		        pthread_mutex_unlock(&mutexRegsPC);
		        ciclosUsados++;
		    	quantum--;
		    	tick();		//simula un ciclo de reloj
		    }
		}
		
		/*
		* Si el valor en el registro RX es un cero, entonces el PC se mueve n instrucciones
		*/
		void BEQZ(int RX, int n) 
		{
		    if(esRegistroValido(RX))
		    {
		    	pthread_mutex_lock(&mutexRegsPC);
			        regsPC[0] += 4;
			        if(regsPC[RX + 1] == 0){
			            regsPC[0] = regsPC[0] + 4*n;
			        }
		        pthread_mutex_unlock(&mutexRegsPC);
		        ciclosUsados++;
		    	quantum--;
		    	tick();		//simula un ciclo de reloj
		    }
		}
		
		/*
		* Si el valor en el registro RX es distinto de cero, entonces el PC se mueve n instrucciones
		*/
		void BNEZ(int RX, int n) 
		{
		    if(esRegistroValido(RX))
		    {
		    	pthread_mutex_lock(&mutexRegsPC);
			        regsPC[0] += 4;
			        if(regsPC[RX + 1] != 0){
			            regsPC[0] = regsPC[0] + 4*n;
			        }
			    pthread_mutex_unlock(&mutexRegsPC);
		        ciclosUsados++;
		    	quantum--;
		    	tick();		//simula un ciclo de reloj
		    }
		}
		
		/*
		* Carga un dato de cache de datos al registro indicado
		*/
		void LW(int RY, int RX, int n)
		{
			int direccion;
			int numBloqCache;
			int numBloqMP;
			int numPal;
			int direcBloqMP;
			bool falloCache = false;
			bool sentinela = false;
			if(esRegistroValido(RY))
			{
				pthread_mutex_lock(&mutexRegsPC);
					direccion = n + regsPC[RY + 1];		//se determina la direccion donde estaría el dato
				pthread_mutex_unlock(&mutexRegsPC);
			}
			//mientras aun no se haya cargado el dato al registro
			while(!sentinela)
			{
				//si se tiene cache local
				if(pthread_mutex_trylock(&mutexCacheDatLocal) == 0)
				{
					numBloqCache = buscarBloqEnCacheDat(direccion);
					//si el bloque esta en la cache
					if(numBloqCache != -1)
					{
						//si el bloque es valido
						if(verificarValidezBloqCacheDat(numBloqCache) == 1)
						{
							numPal = buscarPalEnCacheDat(direccion, numBloqCache);
							pthread_mutex_lock(&mutexRegsPC);
								regsPC[RX + 1] = cacheDat[numPal][numBloqCache];	//se carga el dato al registro
							pthread_mutex_unlock(&mutexRegsPC);
						}
						else	//si el bloque es invalido
						{
							//existe fallo de cache
							falloCache = true;
						}
					}
					else	//si el bloque no esta en cache
					{
						//existe fallo de cache
						falloCache = true;
					}
					//si se produjo un fallo de cache
					if(falloCache)
					{
						//si se tiene el bus
						if(pthread_mutex_trylock(&busDat) == 0)
						{
							numBloqMP = (direccion / 16);
							direcBloqMP = (numBloqMP * 16);
							numPal = (direcBloqMP % 16) / 4;	//numero de palabra en memoria principal
							//sube el bloque de memoria principal de datos a cache de datos
							for(int i = direcBloqMP; i < (direcBloqMP + 16); i += 4)
							{
								cacheDat[numPal][numBloqCache] = memPDatos[i / 4];
							}
							//simula 28 ciclos de reloj
							ticks(28);
							pthread_mutex_unlock(&busDat);
							numPal = buscarPalEnCacheDat(direccion, numBloqCache);	//numero de palabra en cache
							//si el registro destino es valido
							if(esRegDestinoValido(RX))
							{
								pthread_mutex_lock(&mutexRegsPC);
									//carga el dato de la cache de datos al registro indicado
									regsPC[RX + 1] = cacheDat[numPal][numBloqCache];
									regsPC[0] += 4;
								pthread_mutex_unlock(&mutexRegsPC);
							}
							sentinela = true;		//se cargo dato de cache al registro
							quantum--;
						}
						else	//si no se tiene el bus
						{
							ciclosUsados++;
							tick();		//simula un ciclo de reloj
							pthread_mutex_unlock(&mutexCacheDatLocal);
						}
					}
					else	//si no se produjo fallo de cache
					{
						tick();		//simula un ciclo de reloj
						pthread_mutex_unlock(&mutexCacheDatLocal);
					}
				}
			}
			ciclosUsados++;
			tick();		//simula un ciclo de reloj
		}
		
		/*
		* Carga un dato de cache de datos al registro indicado, implementando un candado
		*/
		void LL(int RY, int RX, int n)
		{
			int dir = 0;
			if(esRegistroValido(RY))
			{
				pthread_mutex_lock(&mutexRegsPC);
					dir = n + regsPC[RY + 1];		//se determina la direccion donde estaría el dato
				pthread_mutex_unlock(&mutexRegsPC);
			}
			pthread_mutex_lock(&mutexRegsPC);
				regsPC[33] = dir;	//se guarda direccion en RL
			pthread_mutex_unlock(&mutexRegsPC);
			LW(RY, RX, n);		//se ejecuta el LW normal
		}
		
		/*
		* carga un dato del registro indicado a memoria en la dirección indicada
		*/
		void SW(int RY, int RX, int n)
		{
			int direccion;
			int numBloqCache;
			int numBloqMP;
			int numPal;
			int numPalMP;
			int direcBloqMP;
			bool falloCache = false;
			bool sentinela = false;
			bool cachesRemotasRevisadas = false;
			//si registro fuente es valido
			if(esRegistroValido(RY))
			{
				pthread_mutex_lock(&mutexRegsPC);
					direccion = n + regsPC[RY + 1];		//se determina la dirección de memoria principal donde se guardará el dato
				pthread_mutex_unlock(&mutexRegsPC);
			}
			//mientras no se haya guardado el dato en memoria principal
			while(!sentinela)
			{
				//si se tiene cache local
				if(pthread_mutex_lock(&mutexCacheDatLocal))
				{
					numBloqCache = buscarBloqEnCacheDat(direccion);
					//si el bloque esta en cache local
					if(numBloqCache != -1)
					{
						numPal = buscarPalEnCacheDat(direccion, numBloqCache);
					}
					//si se tiene bus
					if(pthread_mutex_trylock(&busDat) == 0)
					{
						//pedir caches remotas
						for(int g = 0; g < NUM_THREADS; ++g)
						{
							//si no es nucleo local
							if(((long)g) != id)
							{
								pthread_mutex_lock(&mutexVecProcs);
									//si se tiene cache remota
									if(pthread_mutex_trylock(&(vecProcs[g].mutexCacheDatLocal)) == 0)
									{
										//si se puede revisar la ultima cache remota
										if(g == 2)
										{
											cachesRemotasRevisadas = true;	
										}
										numBloqCache = vecProcs[g].buscarBloqEnCacheDat(direccion);
										//si bloque se encuentra en cache
										if(numBloqCache != -1)
										{
											//se invalida bloque en cache remota
											vecProcs[g].cacheDat[5][numBloqCache] = 0;
											//liberar cache remota
											tick();		//simula un ciclo de reloj
											pthread_mutex_unlock(&(vecProcs[g].mutexCacheDatLocal));
											ciclosUsados++;
											pthread_mutex_lock(&(vecProcs[g].mutexRegsPC));
												//si el RL es igual a la direccion donde se guardara el dato
												if(vecProcs[g].regsPC[33] == direccion)
												{
													vecProcs[g].regsPC[33] = -1;
												}
											pthread_mutex_unlock(&(vecProcs[g].mutexRegsPC));
										}
										else	//si el bloque no esta en cache remota
										{
											//liberar cache remota
											tick();		//simula un ciclo de reloj
											pthread_mutex_unlock(&(vecProcs[g].mutexCacheDatLocal));
										}
									}
									else	//si no se obtiene cache remota
									{
										//uso de bus
										tick();		//simula un ciclo de reloj
										pthread_mutex_unlock(&busDat);		//se libera el bus
										ciclosUsados++;
										//uso de cache local
										tick();		//simula un ciclo de reloj
										pthread_mutex_unlock(&mutexCacheDatLocal);
										ciclosUsados++;
									}
								pthread_mutex_unlock(&mutexVecProcs);
							}
							else	//si es el nucleo actual
							{
								//si se revisaron todas las caches remotas
								if(g == 2)
								{
									cachesRemotasRevisadas = true;
								}
							}
							//si se revisaron  e invalidaron las otras caches
							if(cachesRemotasRevisadas)
							{
								pthread_mutex_lock(&mutexRegsPC);
									//copiar dato en registro a palabra en cache de datos
									cacheDat[numPal][numBloqCache] = regsPC[RX + 1];
									numPalMP = (direccion / 4);
									//copiar palabra de cache de datos a memoria principal de datos
									memPDatos[numPalMP] = cacheDat[numPal][numBloqCache];
									regsPC[0] += 4;
								pthread_mutex_unlock(&mutexRegsPC);
								///*
								//simula los ciclos necesarios para copiar una palabra a MP
								ticks(7);
								//*/
								sentinela = true;
								ciclosUsados += 7;
								tick();		//simula un ciclo de reloj
								pthread_mutex_unlock(&busDat);
								tick();		//simula un ciclo de reloj
								pthread_mutex_unlock(&mutexCacheDatLocal);
								ciclosUsados++;
								quantum--;
							}
						}
					}
					else	//si no se tiene el bus
					{
						tick();		//simula un ciclo de reloj
						pthread_mutex_unlock(&mutexCacheDatLocal);
						ciclosUsados++;
					}
				}
			}
			tick();		//simula un ciclo de reloj
		}
		
		/*
		* carga un dato del registro indicado a memoria en la dirección indicada, implementando un candado
		*/
		void SC(int RY, int RX, int n)
		{
			int dir = 0;
			if(esRegistroValido(RY))
			{
				pthread_mutex_lock(&mutexRegsPC);
					dir = n + regsPC[RY + 1];		//se determina la direccion donde estaría el dato
				pthread_mutex_unlock(&mutexRegsPC);
			}
			pthread_mutex_lock(&mutexRegsPC);
				//si RL contiene la direccion donde se guardara el dato
				if(regsPC[33] == dir)
				{
					SW(RY, RX, n);		//se ejecuta el Store modificado, con los RL = -1 en las invalidaciones
					regsPC[33] = -1;	//se pone RL local igual a -1
				}
				else
				{
					regsPC[RX + 1] = 0;	//pone un 0 en el registro destino indicando que no se ejecuto el store
					regsPC[33] = -1;	//se pone RL local igual a -1
				}
			pthread_mutex_unlock(&mutexRegsPC);
		}
		
		/*
		* Guarda el PC actual y luego lo mueve n direcciones
		*/
		void JAL(int n) 
		{
			pthread_mutex_lock(&mutexRegsPC);
			    regsPC[0] += 4;
			    regsPC[31 + 1] = regsPC[0];
			    regsPC[0] += n;
		    pthread_mutex_unlock(&mutexRegsPC);
		    ciclosUsados++;
		    quantum--;
		    tick();		//simula un ciclo de reloj
		}
		
		/*
		* El PC adquiere el valor presente en el registro RX
		*/
		void JR(int RX) 
		{
		    if(esRegistroValido(RX))
		    {
		    	pthread_mutex_lock(&mutexRegsPC);
		        	regsPC[0] = regsPC[RX + 1];
		        pthread_mutex_unlock(&mutexRegsPC);
		        ciclosUsados++;
		        quantum--;
		        tick();		//simula un ciclo de reloj
		    }
		}
		
		/*
		* Pone el estado del hilillo actual como "terminado" en la matriz de hilillos
		*/
		void FIN() 
		{
			pthread_mutex_lock(&mutexRegsPC);
		    	regsPC[0] += 4;
		    pthread_mutex_unlock(&mutexRegsPC);
		    estadoHilillo = 3;
		    ciclosUsados++;
		    quantum--;
		    pthread_mutex_lock(&mutexTiemposXHilillo);
		    	time(&tiempoFin);
				tiemposXHilillo[idHilillo] = (tiempoFin - tiemposXHilillo[idHilillo]);
		    pthread_mutex_unlock(&mutexTiemposXHilillo);
		    tick();		//simula un ciclo de reloj
		}
};
//fin clase Procesador

void* ejecutar(void* numProc)
{
    int numNucleo = *((int*)numProc);
    vecProcs[numNucleo].correr();
}

int inicio()
{
    int rc;
    int cantidadHilillosTerminados;
    pthread_barrier_init(&barrera1,NULL,NUM_THREADS);
    pthread_barrier_init(&barrera2,NULL,NUM_THREADS);
    int* argumento = ((int*)(calloc(1,sizeof(int))));
    if ( argumento == NULL )
    {
        fprintf(stderr, "No se pudo reservar memoria.\n");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < NUM_THREADS; ++i)
    {
        *argumento = i;
        rc = pthread_create(&threads[i],NULL,ejecutar, argumento);
        if(rc)
        {
            std::cout << "Error: imposible crear el hilo" <<std::endl;
            exit(-1);
        }
    }
    free(argumento);
    //pthread_barrier_wait(&barrera1);
    //pthread_barrier_wait(&barrera2);
    //mientras todos los hilillos no terminaron de ejecutarse
    while(!hilillosTerminaron)
    {
        cantidadHilillosTerminados = 0;
        pthread_mutex_lock(&mutexMatrizHilillo);
        for(int t = 0; t < NUM_HILILLOS; ++t)
        {
            if(3 == matrizHilillos[t][35])
            {
                ++cantidadHilillosTerminados;
            }
        }
        if(NUM_HILILLOS == cantidadHilillosTerminados)
        {
            hilillosTerminaron = true;
        }
        pthread_mutex_unlock(&mutexMatrizHilillo);
    }
    //si todos los hilillos terminaron su ejecución
    if(hilillosTerminaron)
    {
        //matar hilos
        for(int j = 0; j < NUM_THREADS; j++)
        {
            pthread_cancel(threads[j]);
        }
    }
    pthread_barrier_destroy(&barrera1);
    pthread_barrier_destroy(&barrera2);
}

//carga hilillos de los archivos de texto a la matriz de contextos
void cargarHilillos()
{
    int v1,v2,v3,v4;
    int direccion;		//indica la proxima dirección en donde guardar una instrucción
    int contadorInstrucciones = 0;	//indica la cantidad de instrucciones ingresadas en memoria actualmente
    for(int i = 0; i < NUM_HILILLOS; ++i)
    {
        bool primerInstr = true;
        int PC;
        std::ifstream flujoEntrada1("0.txt", std::ios::in);
        std::ifstream flujoEntrada2("1.txt", std::ios::in);
        std::ifstream flujoEntrada3("2.txt", std::ios::in);
        std::ifstream flujoEntrada4("3.txt", std::ios::in);
        std::ifstream flujoEntrada5("4.txt", std::ios::in);
        if(!flujoEntrada1 || !flujoEntrada2 || !flujoEntrada3 || !flujoEntrada4 || !flujoEntrada5)
        {
            std::cerr << "No se pudo abrir un archivo" << std::endl;
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
                    matrizHilillos[i][36] = i;
                    primerInstr = false;
                }
                memPInst[PC - 384] = v1;
                memPInst[(PC + 1) - 384] = v2;
                memPInst[(PC + 2) - 384] = v3;
                memPInst[(PC + 3) - 384] = v4;
                contadorInstrucciones++;
            }
            //poner primer hilo en espera
            for(int j = 0; j < 37; ++j)
            {
                colaHilillos[j] = matrizHilillos[i][j];
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
                        matrizHilillos[i][36] = i;
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
                            matrizHilillos[i][36] = i;
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
                                matrizHilillos[i][36] = i;
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
                                    matrizHilillos[i][36] = i;
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
    std::cout << "Memoria principal de Instrucciones:" <<std::endl;
    for(int z = 0; z < TAMA_MPInst; z += 4)
    {
        std::cout << memPInst[z] << "\t" << memPInst[z + 1] << "\t" << memPInst[z + 2] << "\t" << memPInst[z + 3] << "\t" << std::endl;
    }
}

void imprimirMPDat()
{
    std::cout << "Memoria principal de Datos:" <<std::endl;
    for(int z = 0; z < TAM_MPDat; z += 4)
    {
        std::cout << memPDatos[z] << "\t" << memPDatos[z + 1] << "\t" << memPDatos[z + 2] << "\t" << memPDatos[z + 3] << "\t" << std::endl;
    }
}

void imprimirMatrizHilillos()
{
    std::cout << "Matriz de Hilillos:" <<std::endl;
    for(int x = 0; x < NUM_HILILLOS; ++x)
    {
        for(int y = 0; y < 37; ++y)
        {
            std::cout << matrizHilillos[x][y] << "  ";
        }
    }
    std::cout <<std::endl;
}

void imprimirCola()
{
    std::cout << "Cola de Instrucciones:" <<std::endl;
    for(int y = 0; y < 37; ++y)
    {
        std::cout << colaHilillos[y] << "\t";
    }
    std::cout <<std::endl;
}

int main()
{
    cargarHilillos();
    vecProcs.push_back(Procesador(0));
    vecProcs.push_back(Procesador(1));
    vecProcs.push_back(Procesador(2));
    inicio();
    /*
    imprimirMPDat();
    imprimirMPInstr();
    imprimirMatrizHilillos();
    imprimirCola();
    */
}


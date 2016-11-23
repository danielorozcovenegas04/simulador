#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <fstream>
#include <time.h>
#include <vector>
#include <list>

#define NUM_THREADS 3
#define NUM_HILILLOS 7	//numero de hilillos a ejecutar
#define TAMA_MPInst 640 //son 640 celdas porque son 40 bloques y a cada bloque le caben 4 palabras, una palabra es una instruccion,
//y cada palabra tiene 4 enteros
#define TAM_MPDat 96	//son 96 celdas pues son 24 bloques y a cada bloque le caben 4 datos, aunque se simula que cada dato son 4 bytes,
						//al usar enteros solo se necesita un entero para cada dato

int Quantum = 0;	//guardará el quantum ingresado por el usuario
bool hilillosTerminaron = false;	//indica true cuando se terminaron de ejecutar todos los hilillos

int memPDatos[TAM_MPDat];		//vector que representa la memoria principal compartida de datos

int memPInst[TAMA_MPInst];		//vector que representa la memoria principal compartida de instrucciones

struct Contexto
{
	int arreglo[37];	
};

std::vector< Contexto > matrizContextos(NUM_HILILLOS);		//representa los contextos de todos los hilillos y además la cantidad de ciclos de procesamiento
																						//y su estado ("sin comenzar" = 0, "en espera" = 1,"terminado" = 2), y un identificador de hilillo

time_t tiemposXHilillo[NUM_HILILLOS];		//el tiempo real que duro el hilillo en terminar, en un inicio tendra los tiempos en los que cada
											//hilillo se comenzo a ejecutar

std::list<Contexto> colaHilillos;		//cola de hilillos listos para ser ejecutados

time_t tiempoInicio;			//almacenará temporalmente el tiempo en que inicio cada hilillo
time_t tiempoFin;				//almacenará temporalmente el tiempo en que termino cada hilillo

class Procesador;

std::vector<Procesador> vecProcs;  //vector de procesadores o hilos
pthread_t threads[NUM_THREADS];		//identificadores de los hilos

pthread_mutex_t mutexMatrizContextos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCola = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexTiemposXHilillo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t busInst = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t busDat = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexVecProcs = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrera;

//inicio clase Procesador
class Procesador
{
	private:
		long id;                    //identificador del nucleo
		int idHilillo;				//identificador del hilillo actualmente cargado
	    int estadoHilillo;          //tiene un 0 si no ha comenzado su ejecucion, un 1 si esta en espera, un 2 si esta corriendo, y un 3 si ya termino
	    int ciclosUsados;	
		int quantum;
		
		//simula la estructura de una instrucción de MIPS
		struct Instruccion
		{
			int codigoOEtiqOValid;	//contiene el opcode, o la etiqueta, o el bit de validez, dependiendo de la instrucción
			int rf1;				//registro fuente 1
			int rd;					//registro destino
			int rf2Oinmediato;		//registro fuente, o inmediato, dependiendo de la instrucción
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
		
		void setRegsPC(Contexto &pContexto)
		{
			pthread_mutex_lock(&mutexRegsPC);
				for(int i = 0; i < 34; ++i)
				{
					regsPC[i] = pContexto.arreglo[i];
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
			pthread_barrier_wait(&barrera);	//barrera interna
			pthread_barrier_wait(&barrera);	//barrera externa
		}
		
		//simula n ciclos de reloj
		void ticks(int pN)
		{
			for(int i = 0; i < pN; ++i)
			{
				pthread_barrier_wait(&barrera);	//barrera interna
				pthread_barrier_wait(&barrera);	//barrera externa
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
		
		//saca un contexto de la cola de espera y lo guarda en la matriz de contextos
		void sacarDeEspera()
		{
			//copia el contexto del hilillo actualmente en el procesador en la matriz de contextos
			pthread_mutex_lock(&mutexMatrizContextos);
				//guarda los ciclo acumulados en la ejecucion del hilillo
				matrizContextos[idHilillo].arreglo[34] = getCiclos();
				//se pone hilillo como terminado
				matrizContextos[idHilillo].arreglo[35] = 2;
				//pone RL en -1
				matrizContextos[idHilillo].arreglo[33] = -1;
				pthread_mutex_lock(&mutexRegsPC);
					//guarda los valores contenidos en los registros y el PC
					for(int f = 0; f < 34; ++f)
					{
						matrizContextos[idHilillo].arreglo[f] = regsPC[f];
					}
				pthread_mutex_unlock(&mutexRegsPC);
			pthread_mutex_unlock(&mutexMatrizContextos);
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
		            std::cout << "DADDI" <<std::endl;
		            break;
		        case 32:
		            DADD(v2, v3, v4);
		            std::cout << "DADD" <<std::endl;
		            break;
		        case 34:
		            DSUB(v2, v3, v4);
		            std::cout << "DSUB" <<std::endl;
		            break;
		        case 12:
		            DMUL(v2, v3, v4);
		            std::cout << "DMUL" <<std::endl;
		            break;
		        case 14:
		            DDIV(v2, v3, v4);
		            std::cout << "DDIV" <<std::endl;
		            break;
		        case 4:
		            BEQZ(v2, v4);
		            std::cout << "BEQZ" <<std::endl;
		            break;
		        case 5:
		            BNEZ(v2, v4);
		            std::cout << "BNEZ" <<std::endl;
		            break;
		        case 3:
		            JAL(v4);
		            std::cout << "JAL" <<std::endl;
		            break;
		        case 2:
		            JR(v2);
		            std::cout << "JR" <<std::endl;
		            break;
		        case 35:
		            LW(v2, v3, v4);
		            std::cout << "LW" <<std::endl;
		            break;
		        case 50:
		        	LL(v2, v3, v4);
		        	std::cout << "LL" <<std::endl;
		            break;
		        case 43:
		        	SW(v2, v3, v4);
		        	std::cout << "SW" <<std::endl;
		        	break;
		        case 51:
		        	SC(v2, v3, v4);
		        	std::cout << "SC" <<std::endl;
		        	break;
		        case 63:
		            FIN();
		            std::cout << "FIN" <<std::endl;
		            break;
		    }
		}
		
		//carga un hilillo en un hilo para ser ejecutado
		void cargarContexto()
		{
			setQuantum(Quantum);
			setEstadoHilillo(colaHilillos.front().arreglo[35]);
			setCiclos(colaHilillos.front().arreglo[34]);
			setRegsPC(colaHilillos.front());
			//se saca contexto de hilillo de la cola
			colaHilillos.pop_front();
		}
		
		void sacarContexto()
		{
			Contexto contextoTemp;
			//guarda los ciclo acumulados en la ejecucion del hilillo
			contextoTemp.arreglo[34] = getCiclos();
			//si el hilillo aun no termina su ejecucion 
			if(estadoHilillo != 3)
			{
				//se pone en espera en los contextos
				contextoTemp.arreglo[35] = 1;
			}
			pthread_mutex_lock(&mutexRegsPC);
				//carga los valores contenidos en los registros
				for(int f = 0; f < 34; ++f)
				{
					contextoTemp.arreglo[f] = regsPC[f];
				}
			pthread_mutex_unlock(&mutexRegsPC);
			//pone RL en -1
			contextoTemp.arreglo[33] = -1;
			pthread_mutex_lock(&mutexCola);
				colaHilillos.push_back(contextoTemp);
			pthread_mutex_unlock(&mutexCola);
		}
		
		void correr()
		{
			bool hilillosEnEspera;
			int estadoDeProximoHilillo;
			pthread_mutex_lock(&mutexCola);
				hilillosEnEspera = colaHilillos.empty();
			pthread_mutex_unlock(&mutexCola);
			//el hilo correra hasta que no hayan mas hilillos en espera
			while(!hilillosEnEspera)
			{
				pthread_mutex_lock(&mutexCola);
					idHilillo = colaHilillos.front().arreglo[36];
					estadoDeProximoHilillo = colaHilillos.front().arreglo[35];
					//si el proximo hilillo a ejecutar aún no comienza su ejecución o esta en espera
					if(estadoDeProximoHilillo == 0 || estadoDeProximoHilillo == 1)
					{
						//si es la primera vez que se va a poner a correr el hilillo
						if(estadoDeProximoHilillo == 0)
						{
							pthread_mutex_lock(&mutexTiemposXHilillo);
								time(&tiempoInicio);
								tiemposXHilillo[idHilillo] = tiempoInicio;	//guarda el tiempo en que comenzo a ejecutarse el hilillo
							pthread_mutex_unlock(&mutexTiemposXHilillo);
						}
						//se carga el contexto del proximo hilillo al procesador
						cargarContexto();
					}
				pthread_mutex_unlock(&mutexCola);
				while((quantum > 0) && (estadoHilillo != 3))
				{
					correrInstruccion();
				}
				//se mete otra vez a la cola de espera
				pthread_mutex_lock(&mutexMatrizContextos);
					//si aun no termina su ejecucion
					if(matrizContextos[idHilillo].arreglo[35] != 3)
					{
						//se saca el contexto del nucleo y se mete de nuevo al final de la cola
						sacarContexto();
					}
				pthread_mutex_unlock(&mutexMatrizContextos);
				//se revisa si aun existen hilillos sin terminar
				pthread_mutex_lock(&mutexCola);
					hilillosEnEspera = colaHilillos.empty();
				pthread_mutex_unlock(&mutexCola);
				
				//se simula un ciclo de reloj
				tick();
			}
		}
		
		//RT significa cualquier registro
		bool esRegistroValido(int RT) 
		{
		    bool validez = false;
		    if (RT >= 0 && RT < 32) {
		        validez = true;
		    }
		    return validez;
		}
		
		bool esRegDestinoValido(int RT)
		{
		    bool validez = false;
		    if((RT > 0) && (RT < 32))
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
								numPalMP = (direccion / 4);
								pthread_mutex_lock(&mutexRegsPC);
									//si es acierto de escritura Write throgh
									if(numBloqCache != -1)
									{
										//copiar dato en registro a palabra en cache de datos
										cacheDat[numPal][numBloqCache] = regsPC[RX + 1];
										//copiar palabra de cache de datos a memoria principal de datos
										memPDatos[numPalMP] = cacheDat[numPal][numBloqCache];
									}
									else	//si es fallo de escritura No write allocate
									{
										memPDatos[numPalMP] = regsPC[RX + 1];
									}
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
		    //no ingresa el contexto del hilillo de nuevo a la cola porque ya termino su ejecucion
		    sacarDeEspera();
		    tick();		//simula un ciclo de reloj
		}
};
//fin clase Procesador

void* ejecutar(void* numProc)
{
    long numNucleo = (long)numProc;
    vecProcs[numNucleo].correr();
}

int inicio()
{
    int rc;
    int cantidadHilillosTerminados;
    pthread_barrier_init(&barrera,NULL,NUM_THREADS);
    for(int i = 0; i < NUM_THREADS; ++i)
    {
        rc = pthread_create(&threads[i],NULL,ejecutar, (void*)i);
        if(rc)
        {
            std::cout << "Error: imposible crear el hilo" <<std::endl;
            exit(-1);
        }
    }
    //pthread_barrier_wait(&barrera);
    ///*
    //mientras todos los hilillos no terminaron de ejecutarse
    while(!hilillosTerminaron)
    {
        cantidadHilillosTerminados = 0;
        pthread_mutex_lock(&mutexMatrizContextos);
        for(int t = 0; t < NUM_HILILLOS; ++t)
        {
            if(3 == matrizContextos[t].arreglo[35])
            {
                ++cantidadHilillosTerminados;
            }
        }
        if(NUM_HILILLOS == cantidadHilillosTerminados)
        {
            hilillosTerminaron = true;
        }
        pthread_mutex_unlock(&mutexMatrizContextos);
    }
    //*/
    //si todos los hilillos terminaron su ejecución
    if(hilillosTerminaron)
    {
        //matar hilos
        for(int j = 0; j < NUM_THREADS; j++)
        {
            pthread_cancel(threads[j]);
        }
    }
    pthread_barrier_destroy(&barrera);
}

//carga el contexto indicado de la matriz de contextos en la cola
//se ejecuta por el hilo principal
void cargarContextoACola(int pNumeroHilillo)
{
	Contexto contextoTemp;
	for(int r = 0; r < 37; ++r)
	{
		contextoTemp.arreglo[r] = matrizContextos[pNumeroHilillo].arreglo[r];
	}
	colaHilillos.push_back(contextoTemp);
}

//carga hilillos de los archivos de texto a la matriz de contextos
//se ejecuta solo por el hilo principal, no necesita sincronizacion
void cargarHilillos()
{
    int v1,v2,v3,v4;	//son los operandos de las instrucciones
    int direccion;		//indica la proxima dirección en donde guardar una instrucción
    int contadorInstrucciones = 0;	//indica la cantidad de instrucciones ingresadas en memoria actualmente
    bool primerInstr;	//es un sentinela que indica si ya se leyo la primer instruccion del hilillo
    int PC;				//guarda el primer contador del programa para cada hilillo
    for(int i = 0; i < NUM_HILILLOS; ++i)
    {
        primerInstr = true;
        //se cargan los archivos de texto asumiendo que se encuentran en la misma carpeta del projecto
        std::ifstream flujoEntrada1("0.txt", std::ios::in);
        std::ifstream flujoEntrada2("1.txt", std::ios::in);
        std::ifstream flujoEntrada3("2.txt", std::ios::in);
        std::ifstream flujoEntrada4("3.txt", std::ios::in);
        std::ifstream flujoEntrada5("4.txt", std::ios::in);
        std::ifstream flujoEntrada6("5.txt", std::ios::in);
        std::ifstream flujoEntrada7("6.txt", std::ios::in);
        //indica si hubo error al cargar los archivos de texto
        if(!flujoEntrada1 || !flujoEntrada2 || !flujoEntrada3 || !flujoEntrada4 || !flujoEntrada5 || !flujoEntrada6 || !flujoEntrada7)
        {
            std::cerr << "No se pudo abrir un archivo" << std::endl;
            exit(1);
        }
		
		//se revisan todos los archivos de texto
        switch(i)
        {
        	case 0:
        		//mientras queden lineas por leer del archivo
            	while(flujoEntrada1 >> v1 >> v2 >> v3 >> v4)
            	{
            		//se calcula el contador del programa de acuerdo a la cantidad de instrucciones ya ingresadas
            		//y asumiendo que los primeros 20 bloques son de datos
                	PC = 384 + (4 * contadorInstrucciones);
                	//si es la primer instruccion
                	if(primerInstr)
                	{
                		//guardo el contador del programa en la matriz de contextos
                    	matrizContextos[i].arreglo[0] = PC;
                    	//pongo el hilillo en estado "sin comenzar"
                    	matrizContextos[i].arreglo[35] = 0;
                    	//guardo el identificador del hilillo
                    	matrizContextos[i].arreglo[36] = i;
                    	//cargo el contexto a la cola de espera
                    	cargarContextoACola(i);
                    	//se indica que ya se ingreso la primer instruccion de este hilillo
                    	primerInstr = false;
                	}
                	//se copia la instruccion a la memoria principal de instrucciones
                	memPInst[PC - 384] = v1;
                	memPInst[(PC + 1) - 384] = v2;
                	memPInst[(PC + 2) - 384] = v3;
                	memPInst[(PC + 3) - 384] = v4;
                	//se registra el ingreso de una instruccion mas
                	contadorInstrucciones++;
            	}
            	break;
            case 1:
            	while(flujoEntrada2 >> v1 >> v2 >> v3 >> v4)
            	{
                	PC = 384 + (4 * contadorInstrucciones);
                	if(primerInstr)
                	{
                    	matrizContextos[i].arreglo[0] = PC;
                    	matrizContextos[i].arreglo[35] = 0;
                    	matrizContextos[i].arreglo[36] = i;
                    	cargarContextoACola(i);
                    	primerInstr = false;
                	}
                	memPInst[PC - 384] = v1;
                	memPInst[(PC + 1) - 384] = v2;
                	memPInst[(PC + 2) - 384] = v3;
                	memPInst[(PC + 3) - 384] = v4;
                	contadorInstrucciones++;
            	}
            	break;
            case 2:
            	while(flujoEntrada3 >> v1 >> v2 >> v3 >> v4)
            	{
                	PC = 384 + (4 * contadorInstrucciones);
                	if(primerInstr)
                	{
                    	matrizContextos[i].arreglo[0] = PC;
                    	matrizContextos[i].arreglo[35] = 0;
                    	matrizContextos[i].arreglo[36] = i;
                    	cargarContextoACola(i);
                    	primerInstr = false;
                	}
                	memPInst[PC - 384] = v1;
                	memPInst[(PC + 1) - 384] = v2;
                	memPInst[(PC + 2) - 384] = v3;
                	memPInst[(PC + 3) - 384] = v4;
                	contadorInstrucciones++;
            	}
            	break;
            case 3:
            	while(flujoEntrada4 >> v1 >> v2 >> v3 >> v4)
            	{
                	PC = 384 + (4 * contadorInstrucciones);
                	if(primerInstr)
                	{
                    	matrizContextos[i].arreglo[0] = PC;
                    	matrizContextos[i].arreglo[35] = 0;
                    	matrizContextos[i].arreglo[36] = i;
                    	cargarContextoACola(i);
                    	primerInstr = false;
                	}
                	memPInst[PC - 384] = v1;
                	memPInst[(PC + 1) - 384] = v2;
                	memPInst[(PC + 2) - 384] = v3;
                	memPInst[(PC + 3) - 384] = v4;
                	contadorInstrucciones++;
            	}
            	break;
            case 4:
            	while(flujoEntrada5 >> v1 >> v2 >> v3 >> v4)
            	{
                	PC = 384 + (4 * contadorInstrucciones);
                	if(primerInstr)
                	{
                    	matrizContextos[i].arreglo[0] = PC;
                    	matrizContextos[i].arreglo[35] = 0;
                    	matrizContextos[i].arreglo[36] = i;
                    	cargarContextoACola(i);
                    	primerInstr = false;
                	}
                	memPInst[PC - 384] = v1;
                	memPInst[(PC + 1) - 384] = v2;
                	memPInst[(PC + 2) - 384] = v3;
                	memPInst[(PC + 3) - 384] = v4;
                	contadorInstrucciones++;
            	}
            	break;
            case 5:
            	while(flujoEntrada6 >> v1 >> v2 >> v3 >> v4)
            	{
                	PC = 384 + (4 * contadorInstrucciones);
                	if(primerInstr)
                	{
                    	matrizContextos[i].arreglo[0] = PC;
                    	matrizContextos[i].arreglo[35] = 0;
                    	matrizContextos[i].arreglo[36] = i;
                    	cargarContextoACola(i);
                    	primerInstr = false;
                	}
                	memPInst[PC - 384] = v1;
                	memPInst[(PC + 1) - 384] = v2;
                	memPInst[(PC + 2) - 384] = v3;
                	memPInst[(PC + 3) - 384] = v4;
                	contadorInstrucciones++;
            	}
            	break;
            case 6:
            	while(flujoEntrada7 >> v1 >> v2 >> v3 >> v4)
            	{
                	PC = 384 + (4 * contadorInstrucciones);
                	if(primerInstr)
                	{
                    	matrizContextos[i].arreglo[0] = PC;
                    	matrizContextos[i].arreglo[35] = 0;
                    	matrizContextos[i].arreglo[36] = i;
                    	cargarContextoACola(i);
                    	primerInstr = false;
                	}
                	memPInst[PC - 384] = v1;
                	memPInst[(PC + 1) - 384] = v2;
                	memPInst[(PC + 2) - 384] = v3;
                	memPInst[(PC + 3) - 384] = v4;
                	contadorInstrucciones++;
            	}
            	break;
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
    std::cout <<std::endl;
}

void imprimirMPDat()
{
	std::cout <<std::endl;
    std::cout << "Memoria principal de Datos:" <<std::endl;
    for(int z = 0; z < TAM_MPDat; z += 4)
    {
        std::cout << memPDatos[z] << "\t" << memPDatos[z + 1] << "\t" << memPDatos[z + 2] << "\t" << memPDatos[z + 3] << "\t" << std::endl;
    }
    std::cout <<std::endl;
}

void imprimirmatrizContextos()
{
    std::cout << "Matriz de Contextos:" <<std::endl;
    for(int x = 0; x < NUM_HILILLOS; ++x)
    {
        std::cout << "\tContexto #" << x << ":" <<std::endl;
	    std::cout << "\t\t";
		for(int y = 0; y < 37; ++y)
		{
		    std::cout << matrizContextos[x].arreglo[y] << "    ";
		}
		std::cout <<std::endl;
    }
    std::cout <<std::endl;
}

void imprimirCola()
{
	std::list<Contexto> colaTemp(colaHilillos.begin(), colaHilillos.end());
    std::cout << "Cola de Instrucciones:" <<std::endl;
    for(int x = 0; !colaTemp.empty(); ++x)
    {
    	std::cout << "\tContexto #" << x << ":" <<std::endl;
    	std::cout << "\t\t";
	    for(int y = 0; y < 37; ++y)
	    {
	        std::cout << colaTemp.front().arreglo[y] << "    ";
	    }
	    std::cout <<std::endl;
	    colaTemp.pop_front();
    }
}

int main()
{
    cargarHilillos();			//los hilillos deben estar en la carpeta del projecto
    //inicializar memoria compartida de datos con unos
    for(int p = 0; p < TAM_MPDat; ++p)
    {
    	memPDatos[p] = 1;
    }
    vecProcs.push_back(Procesador(0));
    vecProcs.push_back(Procesador(1));
    vecProcs.push_back(Procesador(2));
    //pidiendo el quantum
    std::cout << "Ingrese el quantum: " <<std::endl;
    std::cin >> Quantum;
    //inicio();
    ///*
    imprimirMPDat();
    imprimirMPInstr();
    imprimirmatrizContextos();
    imprimirCola();
    //*/
}


#include "Procesador.h"
#include "pthread.h"

using namespace std;

pthread_mutex_t mutexCacheLocal;
pthread_mutex_t mutexCacheRemotra;

Procesador::Procesador(int pID)
{
    id = pID;
}

Procesador::~Procesador()
{
    
}

void Procesador::setId(long p_Id)
{
    id = p_Id;
}

long Procesador::getId()
{
    return id;
}

void Procesador::setQuantum(int p_quantum)
{
    quantum = p_quantum;
}

int Procesador::getQuantum()
{
    return quantum;
}   

void Procesador::setCiclos(int p_ciclos)
{
    ciclos = p_ciclos;
}

int Procesador::getCiclos()
{
    return ciclos;
}

void Procesador::setRegsPC(int* p_vecRegsPC)
{
    for(int i = 0; i < 34; ++i)
    {
        regsPC[i] = p_vecRegsPC[i];
    }
}

int* Procesador::getRegsPC()
{
    return regsPC;
}

/*
void Procesador::setVecProgs(Procesador* p_vecProgs)
{
    vecProcs = p_vecProgs;
}

Procesador* Procesador::getVecProgs()
{
    return vecProcs;
}
*/

void Procesador::setCacheInst(int* p_cacheInst)
{
    cacheInst = p_cacheInst;
}

int* Procesador::getCacheInst()
{
    return cacheInst;
}

void Procesador::setCacheDat(int* p_cacheDat)
{
    cacheDat = p_cacheDat;
}

int* Procesador::getCacheDat()
{
    return cacheDat;
}

void Procesador::setEstadoHilillo(int p_estado)
{
    estadoHilillo = p_estado;
}

int Procesador::getEstadoHilillo()
{
    return estadoHilillo;
}

void Procesador::setVecProcs(Procesador* pProc1, Procesador* pProc2)
{
    vecProcs[0] = pProc1;
    vecProcs[1] = pProc2;
}

Procesador** getVecProcs()
{
    return vecProcs;
}

void resolverFalloDeCacheInstr()
{
    int numBloqueEnMP = (this.regsPC[0] / 16);
    int numBloqueEnCache = (numBloqueEnMP %  4);
    int direccionEnArregloMPInstr = (numBloqueEnMP * 16) - 384;
    //recorre el bloque en MP de instrucciones y lo copia al bloque correspondiente en caché de instrucciones
    for(int i = direccionEnArregloMPInstr; i < (direccionEnArregloMPInstr + 16); ++i)
    {
        for(int j = 0; j < 4; ++j)
        {
            cacheInst[i % 4][numBloqueEnCache][j] = memPInst[i];
        }
    }
    cacheInst[4][numBloqueEnCache][0] = numBloqueEnMP;
}

void resolverFalloDeCacheDat(int pNumBloqEnMP)
{
    int numBloEnCache = (pNumBloqEnMP % 4);
    
}

//busca el bloque en la cache de instrucciones y retorna el número de bloque en caché de estar valido, en caso de no estar o estar invalido retorna un -1
int buscarBloqEnCacheInstr()
{
    int numBloqueEnMP = (this.regsPC[0] / 16);
    int numBloqueEnCache = (numBloqueEnMP %  4);
    //si el bloque no esta en la caché
    if(cacheInst[4][numBloque] != numBloqueEnMP)
    {
        numBloque = -1;
    }
    return numBloque;
}

//busca la palabra en el bloque y retorna el numero de palabra, retorna -1 en caso de error
int buscarPalEnCacheInstr()
{
    int numPal = -1;
    if(buscarBloqEnCacheInstr() != -1)
    {
        numPal = ((this.regsPC[0] % 16) /  4);
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
void Procesador::correrInstruccion(int* palabra) 
{
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
            
        case 63:
            FIN(v2, v3, v4);
            break;
    }
}

bool Procesador::esRegistroValido(int RX) 
{
    bool validez = false;
    if (RX >= 0 && RX < 32) {
        validez = true;
    }
    return validez;
}

bool Procesador::esRegDestinoValido(int RX)
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
void Procesador::DADDI(int RY, int RX, int n)
{
    //Si el registro RX es el destino, o si uno de los registros no es
    //valido hay error
    if(esRegDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
    {
        Registro[RX] = Registro[RY] + n;    //Realiza el DADDI
        PC +=4;                             //Suma 4 al PC para pasar a la siguiente instruccion
    }
}
    
/**Se encarga de sumar el valor del registro RY con el valor el registro
* RZ, y guardarlo en el registro RX
*/
void Procesador::DADD(int RY, int RZ, int RX)
{
    //Si el registro RX es el destino, o si uno de los registros no es
    //valido hay error
    if(esDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
    {
        Registro[RX] = Registro[RY] + Registro[RZ];    //Realiza el DADDI
        PC +=4;                             //Suma 4 al PC para pasar a la siguiente instruccion
    }
}
    
/*
* Se encarga de restarle al registro RY el valor del registro RZ y 
* almacenarlo en RX
*/
void Procesador::DSUB(int RY, int RZ, int RX)
{
    //Si el registro RX es el destino, o si uno de los registros no es
    //valido hay error
    if(esDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
    {
        Registro[RX] = Registro[RY] - Registro[RZ];    //Realiza el DADDI
        PC +=4;                             //Suma 4 al PC para pasar a la siguiente instruccion
    }
}
    
/*
* Se encarga de multiplicar los registros RY y RZ y guardar el producto en RZ
*/
void Procesador::DMUL(int RY, int RZ, int RX)
{
    //Si el registro RX es el destino, o si uno de los registros no es
    //valido hay error
    if(esDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
    {
        Registro[RX] = Registro[RY] * Registro[RZ];    //Realiza el DADDI
        PC +=4;                             //Suma 4 al PC para pasar a la siguiente instruccion
    }
}
    
/*
* Se encarga de dividir los registros RY y RZ y guardar el producto en RX
*/
void Procesador::DDIV(int RY, int RZ, int RX)
{
    //Si el registro RX es el destino, o si uno de los registros no es
    //valido hay error
    if(esDestinoValido(RX) && esRegistroValido(RX) && esRegistroValido(RY))
    {
        Registro[RX] = Registro[RY] / Registro[RZ];    //Realiza el DADDI
        PC +=4;                             //Suma 4 al PC para pasar a la siguiente instruccion
    }
}

/*
* Si el valor en el registro RX es un cero, entonces el PC se mueve n instrucciones
*/
void Procesador::BEQZ(int RX, int n) 
{
    if(esRegistroValido(RX))
    {
        PC += 4;
        if(Registro[RX] == 0){
            PC = PC + 4*n;
        }
    }
}

/*
* Si el valor en el registro RX es distinto de cero, entonces el PC se mueve n instrucciones
*/
void Procesador::BNEZ(int RX, int n) 
{
    if(esRegistroValido(RX))
    {
        PC += 4;
        if(Registro[RX] != 0){
            PC = PC + 4*n;
        }
    }
}

/*
* Guarda el PC actual y luego lo mueve n direcciones
*/
void Procesador::JAL(int n) 
{
    PC += 4;
    Registro[31] = PC;
    PC += n;
}

/*
* El PC adquiere el valor presente en el registro RX
*/
void Procesador::JR(int RX) 
{
    if(esRegistroValido(RX))
    {
        PC = Registro[RX];
    }
}

/*
* Pone el estado del hilillo actual como "terminado" en la matriz de hilillos
*/
void Procesador::FIN() 
{
    PC += 4;
    estadoHilillo = 3;
}
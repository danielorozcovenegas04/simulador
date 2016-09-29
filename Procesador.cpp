#include "Procesador.h"

Procesador::Procesador(long p_threadId)
{
    id = p_threadId;
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
    regsPC = p_vecRegsPC;
}

int* Procesador::getRegsPC()
{
    return regsPC;
}

void Procesador::setVecProgs(Procesador* p_vecProgs)
{
    vecProcs = p_vecProgs;
}

Procesador* Procesador::getVecProgs()
{
    return vecProcs;
}

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
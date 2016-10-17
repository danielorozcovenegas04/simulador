#include "Procesador.h"

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
    estadoHilillo = 1;
}
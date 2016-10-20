#ifndef PROCESADOR_H
#define PROCESADOR_H

#include <vector>


class Procesador
{
    public:
        Procesador(int);
        virtual ~Procesador();
        void setId(long);
        long getId();
        void setRegsPC(int*);
        int* getRegsPC();
        //void setVecProgs(Procesador*);
        //Procesador* getVecProgs();
        void setCacheInst(int*);
        int* getCacheInst();
        void setCacheDat(int*);
        int* getCacheDat();
        void correrInst(int*);
        bool esRegistroValido(int);
        bool esRegDestinoValido(int);
        void DADDI(int, int, int);
        void DADD(int, int, int);
        void DSUB(int, int, int);
        void DMUL(int, int, int);
        void DDIV(int, int, int);
        void BEQZ(int, int);
        void BNEZ(int, int);
        void JAL(int);
        void JR(int);
        void FIN();
    protected:
    private:
        long id;                    //identificador del nucleo
        int regsPC[34];             //vector con los 32 registros generales, el RL y el PC
        Procesador* vecProcs[2];     //vector con las referencias a los otros procesadores
        int cacheInst[5][4][4];     //matriz que representa la cache de instrucciones, tridimensional porque cada palabra es de cuatro enteros
        int cacheDat[6][4];         //matriz que representa la cache de datos, no es tridimensional porque cada 
                                    //dato en una direccion se interpreta internamente como un solo entero
        int estadoHilillo;          //tiene un 0 si no ha comenzado su ejecucion, un 1 si esta en espera, un 2 si esta corriendo, y un 3 si ya termino
        int ciclosUsados;
};

#endif // PROCESADOR_H
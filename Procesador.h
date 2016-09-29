#ifndef PROCESADOR_H
#define PROCESADOR_H

#include <vector>


class Procesador
{
    public:
        Procesador(long p_threadId);
        virtual ~Procesador();
        void setRegsPC(int*);
        int* getRegsPC();
        void setVecProgs(Procesador*);
        Procesador* getVecProgs();
        void setCacheInst(int*);
        int* getCacheInst();
        void setCacheDat(int*);
        int* getCacheDat();
    protected:
    private:
        int regsPC[34];             //vector con los 32 registros generales, el RL y el PC
        Procesador vecProcs[2];     //vector con las referencias a los otros procesadores
        int cacheInst[6][4][4];     //matriz que representa la cache de instrucciones, tridimensional porque cada palabra es de cuatro enteros
        int cacheDat[6][4];         //matriz que representa la cache de datos, no es tridimensional porque cada 
                                    //dato en una direccion se interpreta internamente como un solo entero
};

#endif // PROCESADOR_H
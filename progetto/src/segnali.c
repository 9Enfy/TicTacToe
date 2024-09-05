/************************************
*VR489009
*Corato Francesco
*
*VR488626
*Sinico Enrico
*
*2024-09-09
*************************************/
#include <../inc/Segnali.h>
#include "../inc/errExit.h"
#include <errno.h>
#include <signal.h>

int ModificaFunzioneSegnale(int sig, void * Function)
{
    if(signal(sig,Function)==SIG_ERR)
    {
        return -1;
    }
    return 0;
}
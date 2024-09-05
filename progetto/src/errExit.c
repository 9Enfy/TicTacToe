/************************************
*VR489009
*Corato Francesco
*
*VR488626
*Sinico Enrico
*
*2024-09-09
*************************************/
#include "errExit.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

void errExit(const char *msg) {
    perror(msg);
    exit(1);
}

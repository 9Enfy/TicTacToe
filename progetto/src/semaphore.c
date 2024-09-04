#include "../inc/semaphore.h"
#include "../inc/errExit.h"
#include <errno.h>
#include <sys/sem.h>

int semOp(int semid, unsigned short sem_num, short sem_op) {
  struct sembuf sop = {.sem_num = sem_num, .sem_op = sem_op, .sem_flg = 0};
  int success;
  while (1) {
    success = semop(semid, &sop, 1);
    if (success == 0)
      return 0;
    if (success == -1 && errno != EINTR) // semaforo si è fermato per qualsiasi motivo tranne è arrivato un segnale esterno. Usa errExit
      return -1;
    // se il semaforo si è fermato perchè ha ricevuto segnale esterno,
    // riattivare semaforo
  }
  return success;
}

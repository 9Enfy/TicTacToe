#include "../inc/SharedMemory.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define IPC_ERROR -1

// file per creare, allocare e distruggere memoria condivisa
static int GetSharedBlock(char *filename, int size) {
  key_t key;
  key = ftok(filename, 0);
  if (key == IPC_ERROR)
    return IPC_ERROR;

  return shmget(key, size, 0644 | IPC_CREAT);
}

InfoGioco *LinkSharedBlock(char *filename, int size) {
  int id = GetSharedBlock(filename, size);
  InfoGioco *result;

  if (id == IPC_ERROR)
    return NULL;
  result = shmat(id, NULL, 0);
  if (result == (InfoGioco *)IPC_ERROR)
    return NULL;
  return result;
}

int UnSharedBlock(InfoGioco *block) { return (shmdt(block) != IPC_ERROR); }

int DestroySharedBlock(char *filename) {
  int id = GetSharedBlock(filename, 0);
  if (id == IPC_ERROR)
    return IPC_ERROR;
  return shmctl(id, 0, NULL) != IPC_ERROR;
}

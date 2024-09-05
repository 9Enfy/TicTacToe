/************************************
*VR489009
*Corato Francesco
*
*VR488626
*Sinico Enrico
*
*2024-09-09
*************************************/
#ifndef SharedMemory
#define SharedMemory
#include <fcntl.h>
/*
 *state indica in che stato si trova il cliente
 * -2 = il server ha appena avviato la memoria condivisa e non conosce ancora i
 *client -1 = il client si è arreso 0 = il client si è connesso. Durente il
 *corso di una partita il client è in questo stato 1 = client ha vinto 2 =
 *client ha perso 3 = client ha pareggiato -4 = timer scaduto. Turno saltato 4 =
 *timer altro giocatore scaduto. Ottieni un turno
 *
 */
struct Giocatore {
  char playerSymbol;
  char playerName[20];
  pid_t playerProcess;
  int state;
} typedef Giocatore;

struct InfoGioco {
  int timeOut;
  Giocatore giocatore1;
  Giocatore giocatore2;
  char Board[3][3];
  int whosTurn;
  pid_t processoServer;

} typedef InfoGioco;

InfoGioco *LinkSharedBlock(char *filename, int size);
int UnSharedBlock(InfoGioco *block);
int DestroySharedBlock(char *filename);

#endif

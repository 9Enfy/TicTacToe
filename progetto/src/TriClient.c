/************************************
*VR489009
*Corato Francesco
*
*VR488626
*Sinico Enrico
*
*2024-09-09
*************************************/
#include "../inc/SharedMemory.h"
#include "../inc/errExit.h"
#include "../inc/semaphore.h"
#include "../inc/Segnali.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

InfoGioco *gioco;
int numGiocatore;
int validMove;
int messaggioDaStampare;
int semaphoreId;
int isPc;
/*
 * Permette al client di disconnettersi quando si preme una volta ctrl+c. Il
 * client si "arrende" e la vincita viene annunciata all'altro client
 *
 */
/*
Un client si può trovare in vari stati (memorizzati in memoria condivisa). Di seguito il significato di ciascun stato
0 = stato di default una volta che il client è inizializzato
1 = il client ha vinto
2 = il client ha perso
3 = il client ha pareggiato
4 = il timer dell'altro client è scaduto
-4 = il timer è scaduto

-1 = il client si è arreso
-2 = il client non è stato ancora inizializzato
-98 = il client richiede al server (attraverso SIGUSR2) di giocare contro un bot
-100 = Richiesto dal server per far diventare il client un bot



*/
void Resa();
void TerminazioneClient(int sig);
void DrawBoard();
void AzioneServer(int sig);
void PcFunction();
void RoutineChiusura(int signal);
void CancellaSchermo();


int main(int argc, char *argv[]) {
  if(argc < 2 || argc > 3){
    printf("Numero argomenti errato: TriClient <nome> <opzioni [\\*, H](facoltativo)>\n");
    RoutineChiusura(-1);
  }
  int isBot=0;
  if(ModificaFunzioneSegnale(SIGUSR1, TerminazioneClient)==-1)
  {
    printf("Errore modifica segnale\n");
    RoutineChiusura(-1);
  }
  if(ModificaFunzioneSegnale(SIGINT, Resa)==-1)
  {
    printf("Errore modifica segnale\n");
    RoutineChiusura(-1);
  }
  if(ModificaFunzioneSegnale(SIGUSR2, AzioneServer)==-1)
  {
    printf("Errore modifica segnale\n");
    RoutineChiusura(-1);
  }
  if(ModificaFunzioneSegnale(SIGHUP, Resa)==-1)
  {
    printf("Errore modifica segnale\n");
    RoutineChiusura(-1);
  }
  key_t semKey = ftok("test", 0);
  if(semKey==-1)
  {
    printf("Errore creazione chiave\n");
    RoutineChiusura(-1);
  }
  gioco = LinkSharedBlock("test", sizeof(InfoGioco));
  if(gioco==NULL)
  {
    printf("Errore ricevimento memoria condivisa\n");
    RoutineChiusura(-1);
  }
  isPc = 0;
  //controlla se il server è attivo
  if(gioco->processoServer==0)
  {
    printf("Server non attivo\n");
    RoutineChiusura(-1);
  }
  
  // controlla se almeno uno dei pidGiocatore della memoria condivisa è uguale
  // ad 1. Se tutti e 2 sono diversi, allora questo è un terzo client
  if (gioco->giocatore1.playerProcess != -1 &&
      gioco->giocatore2.playerProcess != -1) {
    errExit("Ci sono giò 2 client in esecuzione!");
    if(UnSharedBlock(gioco)==-1)
    {
      printf("Errore distaccamento memoria condivisa\n");
      RoutineChiusura(-1);
    }
  }

  semaphoreId = semget(semKey, 7, 0644);
  if(semaphoreId==-1)
  {
    printf("Errore ricevimento semafori\n");
    Resa();
  }
  //il client controlla gli id dei processi client in memoria condivisa. Se un processo è uguale a -1, significa che deve ancora
  //essere inizializzato. Il client quindi memorizza i suoi dati all'interno della area di memoria dedicata al processo.
  if (gioco->giocatore1.playerProcess == -1) {
    gioco->giocatore1.playerProcess = getpid();
    strcpy(gioco->giocatore1.playerName, argv[1]);
    numGiocatore = 2;
    gioco->giocatore1.state = 0;
    
  } else if (gioco->giocatore2.playerProcess == -1) {
    gioco->giocatore2.playerProcess = getpid();
    numGiocatore = 3;
    if(gioco->giocatore2.state==-100)
      isBot=1;
    gioco->giocatore2.state = 0;
    strcpy(gioco->giocatore2.playerName, argv[1]);
  } else {
    printf("Si è verificato un errore\n");
    RoutineChiusura(-1);
  }
  if(semOp(semaphoreId, 1, 1)==-1)
  {
    printf("Errore operazione semafori\n");
    Resa();
  }
  if(semOp(semaphoreId, 0, 1)==-1)
  {
    printf("Errore operazione semafori\n");
    Resa();
  }

  printf("In attesa dell'altro client\n");
  if (argc == 3) // client vuole andare contro il computer
  {
    if (*argv[2] == '*') {
      printf("Hai deciso di andare contro un computer\n");
      //client manda segnale al Server di creare un bot
      if(gioco->giocatore2.playerProcess!=-1)
      {
        printf("Tuttavia c'è già un client in attesa. Giocherai contro un umano\n");
      }
      else
      {
        gioco->giocatore1.state = -98;
        kill(gioco->processoServer,SIGUSR2);
      }
      
    }
    if(*argv[2] == 'H')
    {
      if(gioco->giocatore2.playerProcess==getpid()&&isBot==1)
      {
          PcFunction();
      }else
      {
        printf("Impossibile fare bot vs bot: c'è già un giocatore in attesa\n");
      }
      if(gioco->giocatore2.playerProcess==-1 && gioco->giocatore1.playerProcess==getpid()) //giocatore 1 fa bot vs bot
      {
        gioco->giocatore1.state = -98;
        kill(gioco->processoServer,SIGUSR2);
        PcFunction();
      }

    }
  }
  if(semOp(semaphoreId, 4, 0)==-1)
  {
    printf("Errore operazione semaforo\n");
    Resa();
  }
  printf("Partita iniziata\n");
  while (1) {
    semOp(semaphoreId, numGiocatore, 0);
    CancellaSchermo();
    if (messaggioDaStampare == 1) {
      printf("Scaduto tempo per l'altro client\n");
      messaggioDaStampare = 0;
    }
    printf("E il tuo turno: ");
    if(numGiocatore==2)
    {
      printf("(%c)\n",gioco->giocatore1.playerSymbol);
    }
    if(numGiocatore==3)
    {
      printf("(%c)\n",gioco->giocatore2.playerSymbol);
    }
    DrawBoard();
    int moveMade;
    validMove = -1;
    scanf("%d", &moveMade); 
    if ((!(moveMade < 1 || moveMade > 9))) {
      char charMade = moveMade + 48;
      for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
          if (gioco->Board[i][j] == charMade) {
            validMove = 1;
            if (numGiocatore == 2)
              gioco->Board[i][j] = gioco->giocatore1.playerSymbol;
            if (numGiocatore == 3)
              gioco->Board[i][j] = gioco->giocatore2.playerSymbol;
          }
        }
    }
    CancellaSchermo();
    if (validMove == -1) {
      printf("Mossa non valida, skip turn\n");
    }
    printf("FINE TURNO\n\n");
    printf("-----------------\n");
    DrawBoard();
    if(semOp(semaphoreId, numGiocatore, 1)==-1)
    {
      printf("Errore operazione semaforo\n");
      Resa();
    }
    if(semOp(semaphoreId, 5, 1)==-1)
    {
      printf("Errore operazione semaforo\n");
      Resa();
    }
  }
}

void Resa() {
  semOp(semaphoreId,6,-1);
  printf("Ti sei arreso!\n");
  // manda segnale al server
  
  if (numGiocatore == 2) // giocatore1 si è arreso
  { 
    gioco->giocatore1.state = -1;
  }
  if (numGiocatore == 3) {
    gioco->giocatore2.state = -1;
  }
  semOp(semaphoreId,6,1);
  RoutineChiusura(SIGUSR2);
}

void TerminazioneClient(int sig) {
  CancellaSchermo();
  DrawBoard();
  if (numGiocatore == 2) {
    if (gioco->giocatore1.state == 1) // partita vinta
    {
      printf("Hai vinto contro %s!", gioco->giocatore2.playerName);
      if (gioco->giocatore2.state == 2) // vittoria normale
      {
        printf("\n");
      }
      if (gioco->giocatore2.state == -1) // altro client si è arreso
      {
        printf(" Si è arreso!\n");
      }
      if(gioco->giocatore2.state == 5)
      {
        printf(" Il timer di %s è scaduto\n",gioco->giocatore2.playerName);
      }
    }
    if (gioco->giocatore1.state == 2) // sconfitta
    {
      printf("Hai perso contro %s!\n", gioco->giocatore2.playerName);
    }
    if (gioco->giocatore1.state == 3) {
      printf("Partita finita in pareggio!\n");
    }
    if(gioco->giocatore1.state == 5)
    {
      printf("Il tuo timer è scaduto! Hai perso\n");
    }
  }
  if (numGiocatore == 3 && isPc == 0) {
    if (gioco->giocatore2.state == 1) // partita vinta
    {
      printf("Hai vinto contro %s!", gioco->giocatore1.playerName);
      if (gioco->giocatore1.state == 2) // vittoria normale
      {
        printf("\n");
      }
      if (gioco->giocatore1.state == -1) // altro client si è arreso
      {
        printf(" Si è arreso!\n");
      }
      if(gioco->giocatore1.state == 5)
      {
        printf(" Il timer di %s è scaduto\n",gioco->giocatore1.playerName);
      }
    }
    if (gioco->giocatore2.state == 2) // sconfitta
    {
      printf("Hai perso contro %s!\n", gioco->giocatore1.playerName);
    }
    if (gioco->giocatore2.state == 3) {
      printf("Partita finita in pareggio!\n");
    }
    if(gioco->giocatore2.state == 5)
    {
      printf("Il tuo timer è scaduto! Hai perso\n");
    }
  }
  if (isPc == 0)
    printf("Il server ha terminato la partita. Chiusura client...\n");
  // togli dalla memoria condivisa il tuo pid
  if (numGiocatore == 2) {
    gioco->giocatore1.playerProcess = -1;
  }
  if (numGiocatore == 3) {
    gioco->giocatore2.playerProcess = -1;
  }
  RoutineChiusura(SIGUSR1);
}

void DrawBoard() {
  printf("-------\n");
  printf("|%c|%c|%c|\n", gioco->Board[0][0], gioco->Board[0][1],
         gioco->Board[0][2]);
  printf("|%c|%c|%c|\n", gioco->Board[1][0], gioco->Board[1][1],
         gioco->Board[1][2]);
  printf("|%c|%c|%c|\n", gioco->Board[2][0], gioco->Board[2][1],
         gioco->Board[2][2]);
  printf("-------\n");
}

void AzioneServer(int sig) {
  if (numGiocatore == 2) // giocatore1
  {
    if (gioco->giocatore1.state == 4) // pareggio, tuo turno
    {
      messaggioDaStampare = 1;
    }
    if (gioco->giocatore1.state == -4) {
      printf("Tempo scaduto!\n");
      //int temp = dup(STDIN_FILENO);
      //close(STDIN_FILENO);
      //dup2(temp,STDIN_FILENO);
      semOp(semaphoreId,5,1);
    }
    gioco->giocatore1.state = 0;
  }
  if (numGiocatore == 3) // giocatore2
  {
    if (gioco->giocatore2.state == 4) // pareggio, tuo turno
    {
      messaggioDaStampare = 1;
    }
    if (gioco->giocatore2.state == -4) {
      printf("Tempo scaduto!\n");
      semOp(semaphoreId, 5, 1);
    }
    gioco->giocatore2.state = 0;
  }
}
void PcFunction() {
  semOp(semaphoreId, 4, 0);
  srand(time(NULL));

  while (1) {
    semOp(semaphoreId, numGiocatore, 0);
    validMove = -1;
    // prendi una mossa random, controlla se va bene
    while (validMove == -1) {
      int randomNumber = rand() % 9 + 1;
      char charMade = randomNumber + 48;
      for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
          if (gioco->Board[i][j] == charMade) {
            validMove = 1;
            if (numGiocatore == 2)
              gioco->Board[i][j] = gioco->giocatore1.playerSymbol;
            if (numGiocatore == 3)
              gioco->Board[i][j] = gioco->giocatore2.playerSymbol;
          }
        }
    }
    semOp(semaphoreId, numGiocatore, 1);
    semOp(semaphoreId, 5, 1);
  }
}

void RoutineChiusura(int signal)
{
 
 int detachEffettuato=1;
  if(gioco!=NULL)
  {
    int server = gioco->processoServer;
    if(UnSharedBlock(gioco)==-1)
    {
      printf("Errore chiusura memoria condivisa\n");
      detachEffettuato=0;
    }
    if(signal!=-1)
      kill(server, signal);
  }
  if(detachEffettuato==1)  
    exit(EXIT_SUCCESS);
  exit(EXIT_FAILURE);
}
void CancellaSchermo()
{
  pid_t cancellaId = fork();
  if(cancellaId==-1)
  {
    printf("Errore creazione figlio cancella schermo\n");
    Resa();
  }
  if(cancellaId==0)
  {
    execl("/bin/clear","/bin/clear",(char *)NULL);
  }
  wait(NULL);
}

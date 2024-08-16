#include "../inc/SharedMemory.h"
#include "../inc/errExit.h"
#include "../inc/semaphore.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int cTTimes;
int clientCloseTimes;
InfoGioco *gioco;
key_t semKey;
int semaphoreId;

void StartBoard(InfoGioco *_gioco, char *_argv[]) {
  //_gioco->timeOut = atoi(_argv[1]);
  _gioco->giocatore1.playerSymbol = _argv[2][0];
  _gioco->giocatore2.playerSymbol = _argv[3][0];
  _gioco->Board[0][0] = '1';
  _gioco->Board[0][1] = '2';
  _gioco->Board[0][2] = '3';
  _gioco->Board[1][0] = '4';
  _gioco->Board[1][1] = '5';
  _gioco->Board[1][2] = '6';
  _gioco->Board[2][0] = '7';
  _gioco->Board[2][1] = '8';
  _gioco->Board[2][2] = '9';
  _gioco->whosTurn = 1;
}

int CheckBoard(InfoGioco *gioco) {
  // controlla se qualcuno ha vinto
  if ((gioco->Board[0][0] == gioco->Board[0][1] &&
       gioco->Board[0][1] == gioco->Board[0][2]) ||
      (gioco->Board[1][0] == gioco->Board[1][1] &&
       gioco->Board[1][1] == gioco->Board[1][2]) ||
      (gioco->Board[2][0] == gioco->Board[2][1] &&
       gioco->Board[2][1] == gioco->Board[2][2]) ||
      (gioco->Board[0][0] == gioco->Board[1][0] &&
       gioco->Board[1][0] == gioco->Board[2][0]) ||
      (gioco->Board[0][1] == gioco->Board[1][1] &&
       gioco->Board[1][1] == gioco->Board[2][1]) ||
      (gioco->Board[0][2] == gioco->Board[1][2] &&
       gioco->Board[1][2] == gioco->Board[2][2]) ||
      (gioco->Board[0][0] == gioco->Board[1][1] &&
       gioco->Board[1][1] == gioco->Board[2][2]) ||
      (gioco->Board[0][2] == gioco->Board[1][1] &&
       gioco->Board[1][1] == gioco->Board[2][0]))
    return (gioco->whosTurn * -1);

  // controlla se il campo è pieno
  if (gioco->Board[0][0] != '1' && gioco->Board[0][1] != '2' &&
      gioco->Board[0][2] != '3' && gioco->Board[1][0] != '4' &&
      gioco->Board[1][1] != '5' && gioco->Board[1][2] != '6' &&
      gioco->Board[2][0] != '7' && gioco->Board[2][1] != '8' &&
      gioco->Board[2][2] != '9')
    return 2;
  return 0;
}

void preChiusuraClient(int sig) {
  printf("Un client si è chiuso\n");
  clientCloseTimes++;
}
void RoutineChiusura() {
  // manda ai processi client il segnale di chiusure (SIGUSR1)
  if (gioco->giocatore1.playerProcess != -1)
    kill(gioco->giocatore1.playerProcess, SIGUSR1);
  if (gioco->giocatore2.playerProcess != -1)
    kill(gioco->giocatore2.playerProcess, SIGUSR1);
  // aspetta che entrambi i client si chiudano

  // chiudi aree di memoria utilizzate
  DestroySharedBlock("test");
  // elimina semafori
  semctl(semaphoreId, IPC_RMID,GETALL);
  exit(EXIT_SUCCESS);
}

void preRoutineChiusura(int sig) {

  if (cTTimes == 1) // seconda volta che si preme ctrl+c, il programa termina in
                    // modo sicuro
  {
    system("clear");
    printf("Terminazione server...\n");
    RoutineChiusura();
  }
  if (cTTimes == 0) // prima volta che si preme ctrl+c
  {
    printf("Premere un'altra volta ctrl+c per terminare il server!\n");
    cTTimes = 1;
  }
}

void AzioneClient(int sig) {
  printf("Il client ha eseguito una azione particolare\n");
  // se lo stato di uno dei due client è uguale a -1, allora il client si è
  // arreso, resettare la memoria condivisa del client e dare la vittoria
  // all'altro client
  if (gioco->giocatore1.state == -1) {
    // giocatore1 si è arreso
    printf("giocatore 1 si è arreso");
    gioco->giocatore2.state = 1;
    clientCloseTimes = 1;
    RoutineChiusura();
  }
  if (gioco->giocatore2.state == -1) {
    // giocatore2 si è arreso
    gioco->giocatore1.state = 1;
    clientCloseTimes = 1;
    RoutineChiusura();
  }
  if(gioco->giocatore1.state == -98)
  {
    //giocatore 1 vuole combattere contro un bot
    gioco->giocatore1.state = 0;
    pid_t pid;
    pid = fork();
    if(pid==0)
    {
      //printf("FIGLIO\n");
      
      if(execl("TriClient","TriClient","Bot","H",(char *) NULL)==-1)
      {
        printf("Ouch\n");
      }
    }
    //printf("PADRE\n");
  }
}
void TimerScaduto(int sig) {
  printf("Timer scaduto. Salta turno\n");
  if (gioco->whosTurn == -1) // timer giocatore 1 scaduto
  {
    gioco->giocatore1.state = 1;
      gioco->giocatore2.state = 2;
      
  }
  if (gioco->whosTurn == 1) {
    gioco->giocatore1.state = 2;
    gioco->giocatore2.state = 1;
    
  }
  kill(gioco->giocatore1.playerProcess, SIGUSR1);
  kill(gioco->giocatore2.playerProcess, SIGUSR1);
  RoutineChiusura();

}
int main(int argc, char *argv[]) {
  // controlla se il numero di parametri è giusto
  if (argc != 4) {
    printf("Errore: errato numero di parametri. \n ./TriServer [secondi di "
           "timeout] [carattere per giocatore 1] [carattere per giocatore 2]");
    return -1;
  }
  // gestione segnali
  cTTimes = 0;
  clientCloseTimes = 0;
  signal(SIGINT, *preRoutineChiusura);
  signal(SIGUSR1, *preChiusuraClient);
  signal(SIGUSR2, *AzioneClient);
  signal(SIGALRM, *TimerScaduto);
  int timer = atoi(argv[1]);
  if (timer < 0) {
    printf("Timer può essere solo positivo!");
    exit(EXIT_FAILURE);
  }
  // crea N[] semafori. Saranno subito inizializzati ad 1
  semKey = ftok("test", 1);
  semaphoreId = semget(semKey, 6, 0644 | IPC_CREAT);

  /*
   *sem[0] viene usato dal server per aspettare il collegamento di 2 client
   *sem[1] viene usato dai client per inizializzare la connessione con il server
   *sem[2] è gestito dal server e comunica al client 1 quando può mettere in
   *input una mossa
   *sem[3] è gestito dal server e comunica al client 2 quando
   *può mettere in input una mossa
   *sem[4] usato dai 2 client. Attendono l'inizio della partita
   *sem[5] usato dai client quando eseguono azioni particolari.
   */

  if (semaphoreId == -1)
    errExit("semget failed");
  unsigned short semInitVal[] = {0, 0, 1, 1, 1, 0};
  union semun arg;
  arg.array = semInitVal;

  if (semctl(semaphoreId, 0 /*ignored*/, SETALL, arg) == -1)
    errExit("semctl SETALL failed");

  // crea chiave
  // mkdir("tmp", S_IRUSR | S_IXUSR);
  // int fd = open("./tmp/chiave", O_RDWR | O_CREAT | O_EXCL);

  // inizializza memoria condivisa
  gioco = LinkSharedBlock("test", sizeof(InfoGioco));
  // inizializza campo di gioco
  StartBoard(gioco, argv);
  // salva in memoria condivisa il pid del server. Utile per far si che i client
  // possano mandare segnali al server
  gioco->processoServer = getpid();
  // il server non conosce i pid dei client, imposta a -1 i pid dei client
  gioco->giocatore1.playerProcess = -1;
  gioco->giocatore2.playerProcess = -1;

  // server imposta a -2 lo stato dei client
  gioco->giocatore1.state = -2;
  gioco->giocatore2.state = -2;

  // INIZIALIZZAZIONE FINITA,
  semOp(semaphoreId, 1, 1);
  // il server riprende esecuzione quando sem[0] è uguale a 0 (nei client viene chiamata 2 volte semOP(0))
  semOp(semaphoreId, 0, -2);
  printf("INIZIALIZZAZIONE CLIENT FINITA\n");
  semOp(semaphoreId, 4, -1);

  // inizia la partita
  int isThereAWinner;
  while (1) {
    // controlla il campo per determinare fine del gioco
    isThereAWinner = CheckBoard(gioco);
    if (isThereAWinner == 1) {
      // giocatore 1 ha vinto
      printf("%s ha vinto. Chiusura programma\n", gioco->giocatore1.playerName);
      gioco->giocatore1.state = 1;
      gioco->giocatore2.state = 2;
      kill(gioco->giocatore2.playerProcess, SIGUSR1);
      RoutineChiusura();
    }
    if (isThereAWinner == -1) {
      // giocatore 2 ha vinto
      printf("%s ha vinto. Chiusura programma\n", gioco->giocatore2.playerName);
      gioco->giocatore1.state = 2;
      gioco->giocatore2.state = 1;
      kill(gioco->giocatore2.playerProcess, SIGUSR1);
      RoutineChiusura();
      return 0;
    }
    if (isThereAWinner == 2) {
      // campo pieno, parità
      printf("Parita!. Chisura programma\n");
      gioco->giocatore1.state = 3;
      gioco->giocatore2.state = 3;
      kill(gioco->giocatore1.playerProcess, SIGUSR1);
      kill(gioco->giocatore2.playerProcess, SIGUSR1);
      RoutineChiusura();
    }
    // partita continua
    if (gioco->whosTurn == 1) {
      // turno del giocatore1;
      semOp(semaphoreId, 2, -1);
    }
    if (gioco->whosTurn == -1) {
      // turno del giocatore2
      semOp(semaphoreId, 3, -1);
    }
    alarm(timer);
    semOp(semaphoreId, 5, -1);
    alarm(0);
    gioco->whosTurn *= -1;
    printf("Nuovo turno");
  }

  return 0;
}

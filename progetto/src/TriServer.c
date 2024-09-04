#include "../inc/SharedMemory.h"
#include "../inc/errExit.h"
#include "../inc/semaphore.h"
#include "../inc/Segnali.h"
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
#include <sys/wait.h>

int cTTimes;
int clientCloseTimes;
InfoGioco *gioco;
key_t semKey;
int semaphoreId;
int privateSemaphore;


void StartBoard(InfoGioco *_gioco, char *_argv[]);
int CheckBoard(InfoGioco *gioco);
void preChiusuraClient(int sig);
void RoutineChiusura();
void StampaEChiudiErrore(char*reason);
void preRoutineChiusura(int sig);
void AzioneClient(int sig);
void TimerScaduto(int sig);
void CancellaSchermo();

int main(int argc, char *argv[]) {
  // controlla se il numero di parametri è giusto
  if (argc != 4) {
    StampaEChiudiErrore("Errore: errato numero di parametri. \n ./TriServer [secondi di timeout] [carattere per giocatore 1] [carattere per giocatore 2]");
  }
  semKey = ftok("test", 0);
  if(semKey==-1)
  {
    StampaEChiudiErrore("Errore inzializzazione chiave\n");
  }
  
  //cancella le IPC con la stessa chiave semKey già presenti nel sistema
  char numberArgBuffer[20];    // more than big enough for a 32 bit integer
  snprintf(numberArgBuffer, 20, "%#010x", semKey);
  pid_t figlioBash = fork();
  if(figlioBash==-1)
  {
    StampaEChiudiErrore("Errore creazione figlio per eliminare semafori inizio\n");
  }
  if(figlioBash==0)
  {
    if(execl("/bin/ipcrm","/bin/ipcrm","-S",numberArgBuffer,(char *) NULL)==-1)
    {
      StampaEChiudiErrore("Errore esecuzione comando per eliminare semafori inizio\n");
    }
  }
  wait(NULL);
  figlioBash=fork();
  if(figlioBash==-1)
  {
    StampaEChiudiErrore("Errore creazione figlio per eliminare memoria inizio\n");
  }
  if(figlioBash==0)
  {
    if(execl("/bin/ipcrm","/bin/ipcrm","-M",numberArgBuffer,(char *) NULL)==-1)
    {
      StampaEChiudiErrore("Errore esecuzione comando per eliminare memoria inizio\n");
    }
  }
  wait(NULL);
  
  // gestione segnali
  cTTimes = 0;
  clientCloseTimes = 0;
  if(ModificaFunzioneSegnale(SIGINT, *preRoutineChiusura)==-1)
  {
    StampaEChiudiErrore("Errore modifica SIGINT");
  }
  if(ModificaFunzioneSegnale(SIGUSR1, *preChiusuraClient)==-1)
  {
    StampaEChiudiErrore("Errore modifica SIGUSR1");
  }
  if(ModificaFunzioneSegnale(SIGUSR2, *AzioneClient)==-1)
  {
    StampaEChiudiErrore("Errore modifica SIGUSR2");
  }
  if(ModificaFunzioneSegnale(SIGALRM, *TimerScaduto)==-1)
  {
    StampaEChiudiErrore("Errore modifica SIGALRM");
  }
  int timer = atoi(argv[1]);
  if (timer < 0) {
    StampaEChiudiErrore("Timer può essere solo positivo!");
  }
  // crea N[] semafori. Saranno subito inizializzati ad 1
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
    StampaEChiudiErrore("semget failed");
  unsigned short semInitVal[] = {0, 0, 1, 1, 1, 0};
  union semun arg;
  arg.array = semInitVal;

  if (semctl(semaphoreId, 0 /*ignored*/, SETALL, arg) == -1)
    StampaEChiudiErrore("semctl SETALL failed");

  // crea chiave
  // mkdir("tmp", S_IRUSR | S_IXUSR);
  // int fd = open("./tmp/chiave", O_RDWR | O_CREAT | O_EXCL);

  // inizializza memoria condivisa
  gioco = LinkSharedBlock("test", sizeof(InfoGioco));
  if(gioco==NULL)
  {
    StampaEChiudiErrore("Errore nella creazione memoria condivisa");
  }
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
  if(semOp(semaphoreId, 1, 1)==-1)
  {
    StampaEChiudiErrore("Errore operazione con semaforo");
  }
  // il server riprende esecuzione quando sem[0] è uguale a 0 (nei client viene chiamata 2 volte semOP(0))
  if(semOp(semaphoreId, 0, -2)==-1)
  {
    StampaEChiudiErrore("Errore operazione con semaforo");
  }
  printf("INIZIALIZZAZIONE CLIENT FINITA\n");
  if(semOp(semaphoreId, 4, -1)==-1)
  {
    StampaEChiudiErrore("Errore operazione con semaforo");
  }

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
      if(semOp(semaphoreId, 2, -1)==-1)
      {
        StampaEChiudiErrore("Errore operazione con semaforo");
      }
    }
    if (gioco->whosTurn == -1) {
      // turno del giocatore2
      if(semOp(semaphoreId, 3, -1)==-1)
      {
        StampaEChiudiErrore("Errore operazione con semaforo");
      }
    }
    alarm(timer);
    if(semOp(semaphoreId, 5, -1))
    {
      StampaEChiudiErrore("Errore operazione con semaforo");
    }
    alarm(0);
    gioco->whosTurn *= -1;
    printf("Nuovo turno\n");
  }

  return 0;
}

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
  if(DestroySharedBlock("test")==-1)
  {
    perror("Errore distruzione memoria");
    exit(EXIT_FAILURE);
  }
  // elimina semafori
  if(semctl(semaphoreId, 0, IPC_RMID)==-1)
  {
    perror("Errore distruzione semafori");
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
void StampaEChiudiErrore(char*reason)
{
  perror(reason);
  RoutineChiusura();
}

void preRoutineChiusura(int sig) {

  if (cTTimes == 1) // seconda volta che si preme ctrl+c, il programa termina in
                    // modo sicuro
  {
    CancellaSchermo();
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
        StampaEChiudiErrore("Errore creazione Bot\n");
      }
    }
    if(pid==-1)
    {
      StampaEChiudiErrore("Errore creazione Bot");
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
void CancellaSchermo()
{
  pid_t cancellaId = fork();
  if(cancellaId==-1)
  {
    StampaEChiudiErrore("Errore creazione figlio cancella schermo");
  }
  if(cancellaId==0)
  {
    execl("/bin/clear","/bin/clear",(char *)NULL);
  }
  wait(NULL);
}

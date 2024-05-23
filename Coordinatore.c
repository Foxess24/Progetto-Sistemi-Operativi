#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

#define MAX_VISUALIZZATORI 10
#define NUMERO_SHM "/numero_shm"
#define SEMAFORO_SHM "/semaforo_shm"
#define SEMAFORO_SINCRO "/semaforo_sincro"

/* ---  SEGNALI  --- */
// Gestione segnale SIGALRM (comuniczione)
void gestore_SIGALRM(int sig);
// Gestione segnale SIGUSR1 (sospensione)
void gestore_SIGUSR1(int sig);
// Gestione segnale SIGUSR2 (ripresa)
void gestore_SIGUSR2(int sig);
// Gestione segnale SIGTERM (terminazione)
void gestore_SIGTERM(int sig);
// Gestione segnale SIGINT (ignora)
void gestore_SIGINT(int sig);

// Chiusura + pulizia
void end(int fd_shm, int *numero_shm, sem_t *semaforo_shm, sem_t *semaforo_sincro, pid_t visualizzatori[], int num_v);

// Variabile globale per controllo comunicazione con visualizzatore
volatile sig_atomic_t sigArrivato = 0;
// Variabile globale per controllo sospensione del programma
volatile sig_atomic_t inPausa = 0;

// Variabili
int num_v; // numero di visualizzatori
int N; // numero dato da utente
pid_t visualizzatori[MAX_VISUALIZZATORI]; // array di PID dei visualizzatori
int fd_shm = -1; // fd per shm
int *numero_shm = NULL; // memoria condivisa - shm
sem_t *semaforo_shm = NULL; // semaforo su shm
sem_t *semaforo_sincro = NULL; // semaforo per sincronizzare l'inizio


int main(int argc, char *argv[]) {
    pid_t pid;
  
    if (argc != 3) {
        fprintf(stderr, "Inserisci: %s <num_visualizzatori> <N>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get parametri
    num_v = atoi(argv[1]);
    N = atoi(argv[2]);

    // Check parametri
    if (num_v <= 0 || num_v > MAX_VISUALIZZATORI || N <= 0) {
        fprintf(stderr, "Argomenti non validi: %s <num_visualizzatori[1-10]> <N[1-]>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Creazione della memoria condivisa
    fd_shm = shm_open(NUMERO_SHM, O_CREAT | O_RDWR, 0666);
    if (fd_shm == -1) {
        perror("Errore open/create shm!\n");
        exit(EXIT_FAILURE);
    }

    // Dimensione shm pari alla dimensione di un INT
    if (ftruncate(fd_shm, sizeof(int)) == -1) {
        perror("Errore truncate shm!\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }

    // Mappa la memoria
    numero_shm = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if (numero_shm == MAP_FAILED) {
        perror("Errore in nmap!\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }

    // Semaforo shm
    semaforo_shm = sem_open(SEMAFORO_SHM, O_CREAT, 0666, 1);
    if (semaforo_shm == SEM_FAILED) {
        perror("Errore nella creazione del semaforo shm!\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }

    // Semaforo sincronizzazione
    semaforo_sincro = sem_open(SEMAFORO_SINCRO, O_CREAT, 0666, 0);
    if (semaforo_sincro == SEM_FAILED) {
        perror("Errore nella creazione del semaforo sincro!\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }

    // Inizializzazione shm a 1
    sem_wait(semaforo_shm);
    *numero_shm = 1;
    sem_post(semaforo_shm);

    // Impostazione gestore per SIGALRM (comunicazione)
    struct sigaction sigALRM;
    sigALRM.sa_handler = gestore_SIGALRM;
    sigALRM.sa_flags = SA_RESTART; // evita EINTR
    sigemptyset(&sigALRM.sa_mask);
    if (sigaction(SIGALRM, &sigALRM, NULL) == -1) {
        perror("Errore nella sigaction di SIGALRM\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }

    // Impostazione gestore per SIGUSR1 (sospensione) 
    struct sigaction sigUSR1;
    sigUSR1.sa_handler = gestore_SIGUSR1;
    sigUSR1.sa_flags = SA_RESTART; // evita EINTR
    sigemptyset(&sigUSR1.sa_mask);
    if (sigaction(SIGUSR1, &sigUSR1, NULL) == -1) {
        perror("Errore nella sigaction di SIGUSR1\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }

    // Impostazione gestore per SIGUSR2 (ripresa) 
    struct sigaction sigUSR2;
    sigUSR2.sa_handler = gestore_SIGUSR2;
    sigUSR2.sa_flags = SA_RESTART; // evita EINTR
    sigemptyset(&sigUSR2.sa_mask);
    if (sigaction(SIGUSR2, &sigUSR2, NULL) == -1) {
        perror("Errore nella sigaction di SIGUSR2\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }

    // Impostazione gestore per SIGTERM (terminazione)
    struct sigaction sigTERM;
    sigTERM.sa_handler = gestore_SIGTERM;
    sigTERM.sa_flags = SA_RESTART; // evita EINTR
    sigemptyset(&sigTERM.sa_mask);
    if (sigaction(SIGTERM, &sigTERM, NULL) == -1) {
        perror("Errore nella sigaction di SIGTERM\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }

    // Impostazione gestore per SIGINT (ignora) 
    struct sigaction sigINT;
    sigINT.sa_handler = gestore_SIGINT;
    sigINT.sa_flags = SA_RESTART; // evita EINTR
    sigemptyset(&sigINT.sa_mask);
    if (sigaction(SIGINT, &sigINT, NULL) == -1) {
        perror("Errore nella sigaction di SIGINT\n");
        end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
        exit(EXIT_FAILURE);
    }


    // Creazione processi visualizzatori
    for (int i = 0; i < num_v; i++) {
        pid = fork();
        if (pid == -1) {
            perror("Errore nella fork()!\n");
            end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Codice del processo visualizzatore
            execlp("./outVisualizzatore", "./outVisualizzatore", NULL);
            perror("execlp");
            exit(EXIT_FAILURE);
        } else {
            // Salvo pid del visualizzatore
            visualizzatori[i] = pid;
        }
    }

    // Attesa che tutti i visualizzatori siano pronti
    for (int i = 0; i < num_v; i++) {
       sem_wait(semaforo_sincro);
    }

    // Ciclo di lavoro
    srand(time(NULL));
    for (int i = 0; i < N; i++) {
       while(inPausa == 1){
            sigset_t mask1, oldmask1;
            sigemptyset(&mask1);
            sigaddset(&mask1, SIGUSR2);
            sigprocmask(SIG_BLOCK, &mask1, &oldmask1);
            sigsuspend(&oldmask1); // Attende specificamente SIGUSR2
            sigprocmask(SIG_SETMASK, &oldmask1, NULL);
         }

         // Estraggo visualizzatore
         int vis_scelto = rand() % num_v;
         printf("C: V[%d] estratto\n", visualizzatori[vis_scelto]);
         
         // Blocca SIGALRM
         sigset_t mask, oldmask;
         sigemptyset(&mask);
         sigaddset(&mask, SIGALRM);
         sigprocmask(SIG_BLOCK, &mask, &oldmask);

         // Invio segnale di comunicazione a visualizzatore estratto
         kill(visualizzatori[vis_scelto], SIGALRM);
 
         // Attesa risposta
         sigArrivato = 0;
         while (sigArrivato == 0) {
             sigsuspend(&oldmask);
         }

         // Ripristina la vecchia maschera dei segnali
         sigprocmask(SIG_SETMASK, &oldmask, NULL);

         // Inizio sezione critica: blocco semaforo
         sem_wait(semaforo_shm);

         // Registro PID visualizzatore in coord_pid.txt
         FILE *file = fopen("coord_pid.txt", "a");
         if (file == NULL) {
             perror("fopen");
             end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
             exit(EXIT_FAILURE);
          }

          // Leggo shm e prendo [value(shm) - 1] poichè il visualizzatore l'ha icrementata
          fprintf(file, "C: V[%d] --> {%d} shm\n", visualizzatori[vis_scelto], *numero_shm-1);
          
          fclose(file);

          // Fine sezione critica: rilascio semaforo
          sem_post(semaforo_shm);
    }
    
    // Terminazione + pulizia
    end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);

    return 0;
}

// Gestore SIGALRM (comunicazione)
void gestore_SIGALRM(int sig){
    sigArrivato = 1;
}

// Gestore SIGUSR1 (sospendi)
void gestore_SIGUSR1(int sig) {
    printf("Comando ricevuto SIGUSR1: Sospendi visualizzazione\n");
    inPausa = 1;
}

// Gestore SIGUSR2 (riprendi)
void gestore_SIGUSR2(int sig) {
    printf("Comando ricevuto SIGUSR2: Riprendi visualizzazione\n");
    inPausa = 0;
}

// Gestore SIGTERM (termina)
void gestore_SIGTERM(int sig) {
    printf("Comando ricevuto SIGTERM: Termina applicazione\n");
    end(fd_shm, numero_shm, semaforo_shm, semaforo_sincro, visualizzatori, num_v);
    exit(EXIT_SUCCESS);
}

// Gestore SIGINT (ignora)
void gestore_SIGINT(int sig) {
    printf("Comando ricevuto SIGINT: ignorato\n");
}

// Rilascio risorse
void end(int fd_shm, int *numero_shm, sem_t *semaforo_shm, sem_t *semaforo_sincro, pid_t visualizzatori[], int num_v) {
    // Attesa della terminazione dei processi visualizzatori
    for (int i = 0; i < num_v; i++) {
        if (visualizzatori[i] > 0) {
            kill(visualizzatori[i], SIGTERM);
            waitpid(visualizzatori[i], NULL, 0);
        }
    }

    // Dealloca la mappatura della memoria condivisa
    if (numero_shm && munmap(numero_shm, sizeof(int)) == -1) {
        perror("Errore in munmap della shm!\n");
    }

    // Rimozione della memoria condivisa
    if (fd_shm != -1 && close(fd_shm) == -1) {
        perror("Errore in close fd_shm!\n");
    }

    if (shm_unlink(NUMERO_SHM) == -1) {
        perror("Errore in unlink NUMERO_SHM!\n");
    }

    // Chiusura e rimozione dei semafori
    if (semaforo_shm && sem_close(semaforo_shm) == -1) {
        perror("Errore in close semaforo_shm!\n");
    }

    if (sem_unlink(SEMAFORO_SHM) == -1) {
        perror("Errore in unlink SEMAFORO_SHM!\n");
    }

    if (semaforo_sincro && sem_close(semaforo_sincro) == -1) {
        perror("Errore in close semaforo_sincro!\n");
    }

    if (sem_unlink(SEMAFORO_SINCRO) == -1) {
        perror("Errore in unlink SEMAFORO_SINCRO!\n");
    }
}
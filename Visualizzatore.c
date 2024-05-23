#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>

#define NUMERO_SHM "/numero_shm"
#define SEMAFORO_SHM "/semaforo_shm"
#define SEMAFORO_SINCRO "/semaforo_sincro"

/* ---  SEGNALI  --- */
// Gestione segnale SIGALRM (comuniczione)
void gestore_SIGALRM(int sig);
// Gestione segnale SIGTERM (terminazione)
void gestore_SIGTERM(int sig);
// Gestione segnale SIGINT (ignora)
void gestore_SIGINT(int sig);

// Variabili globali
int *numero_shm; // memoria condivisa
sem_t *semaforo_shm; // semaforo su shm
sem_t *semaforo_sincro; // semaforo per sincronizzare l'inizio
int fd_shm; // file descriptor per shm
FILE *file = NULL; // file

int main() {

    // Collegamento alla memoria condivisa
    fd_shm = shm_open(NUMERO_SHM, O_RDWR, 0666);
    if (fd_shm == -1) {
        perror("Errore in apertura shm!\n");
        exit(EXIT_FAILURE);
    }

    numero_shm = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if (numero_shm == MAP_FAILED) {
        perror("Errore in mappatura shm!\n");
        exit(EXIT_FAILURE);
    }

    // Collegamento al semaforo shm
    semaforo_shm = sem_open(SEMAFORO_SHM, 0);
    if (semaforo_shm == SEM_FAILED) {
        perror("Errore apertura del semaforo!\n");
        exit(EXIT_FAILURE);
    }
    
    // Collegamento al semaforo sincro
    semaforo_sincro = sem_open(SEMAFORO_SINCRO, 0);
    if (semaforo_sincro == SEM_FAILED) {
        perror("Errore apertura semaforo sincro!\n");
        munmap(numero_shm, sizeof(int));
        close(fd_shm);
        sem_close(semaforo_shm);
        exit(EXIT_FAILURE);
    }

    // Gestore SIGALRM
    struct sigaction sigALRM;
    sigALRM.sa_handler = gestore_SIGALRM;
    sigALRM.sa_flags = 0;
    sigemptyset(&sigALRM.sa_mask);
    if (sigaction(SIGALRM, &sigALRM, NULL) == -1) {
        perror("Errore nella sigaction ALRM!\n");
        exit(EXIT_FAILURE);
    }

    // Gestore SIGTERM
    struct sigaction sigTERM;
    sigTERM.sa_handler = gestore_SIGTERM;
    sigTERM.sa_flags = 0;
    sigemptyset(&sigTERM.sa_mask);
    if (sigaction(SIGTERM, &sigTERM, NULL) == -1) {
        perror("Errore nella sigaction TERM!\n");
        exit(EXIT_FAILURE);
    }

    // Impostazione gestore per SIGINT (ignora) 
    struct sigaction sigINT;
    sigINT.sa_handler = gestore_SIGINT;
    sigINT.sa_flags = 0;
    sigemptyset(&sigINT.sa_mask);
    if (sigaction(SIGINT, &sigINT, NULL) == -1) {
        perror("Errore nella sigaction di SIGINT\n");
        exit(EXIT_FAILURE);
    }


    // Pronto per ricevere segnali
    sem_post(semaforo_sincro);

    // Attesa segnale
    while (1) {
        pause();
    }

    return 0;
}

void gestore_SIGALRM(int sig) {
    // Inizio sezione critica: blocco semaforo
    sem_wait(semaforo_shm);

    // Open file
    file = fopen("vis_pid.txt", "a");
    if (file == NULL) {
        perror("Errore apertura file!\n");
        //sem_post(semaforo_shm);
        return;
    }
    
    // Write to file
    fprintf(file, "V: V[%d] --> {%d} shm\n", getpid(), *numero_shm);
    printf("V[%d]: shm --> %d\n", getpid(), *numero_shm);

    // Incremento di 1 shm
    (*numero_shm)++;

    // Close file
    if (fclose(file) != 0) {
        perror("Errore chiusura file!\n");
        sem_post(semaforo_shm);
        return;
    }  
    
    // Fine sezione critica: rilascio semaforo
    sem_post(semaforo_shm);

    // Invio segnale di terminazione
    kill(getppid(), SIGALRM);
}

void gestore_SIGTERM(int sig) {
    // Pulizia
    munmap(numero_shm, sizeof(int));
    sem_close(semaforo_shm);
    sem_close(semaforo_sincro);
    exit(0);
}

// Gestore SIGINT (ignora)
void gestore_SIGINT(int sig) {
    printf("Visualizzatore--> comando ricevuto SIGINT: ignorato\n");
}
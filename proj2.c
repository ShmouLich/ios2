//
// IOS PROJEKT 2
// Created by Ondřej Kříž on 21.04.21.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define sem_fail() fprintf(stderr, "chyba pri inicializaci semaforu\n"); exit(1)

typedef struct count {
    int process_id;             //cislo provadeneho procesu
    int elves_to_help;          //pocet elfu cekajicich na pomoc santy
    int rndrs_back;             //pocet sobu navracenych z dovolene
    int hitched_rndrs;          //pocet zaprahnutych sobu
    bool workshop_not_closed;   //na zacatku programu true
} count_t;

int elf_id, rndr_id, santa_id;

sem_t *santa_sem, *elf_sem, *rndr_sem, *mutex; //semafory

FILE *out; //soubor do ktereho je zapsan vystup programu
int ne, nr, te, tr; //pocet elfu, sobu, max. doby prace elfa, dovolene soba

//vrati cas spanku v mikrosekundach pro funkci usleep
int wait_usleep(int min, int max) {
    srand(time(0));
    int ret = (rand() % (max + 1)) * 1000;
    while (ret < min) {
        ret += 1000;
    }
    return ret;
}

void init_semaphores() {
    if((santa_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED){
        sem_fail();
    }
    if((elf_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED){
        sem_fail();
    }
    if((rndr_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED){
        sem_fail();
    }
    if((mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED){
        sem_fail();
    }
    if (sem_init(santa_sem, 1, 0) == -1 ||
        sem_init(elf_sem, 1, 0) == -1 ||
        sem_init(rndr_sem, 1, 0) == -1 ||
        sem_init(mutex, 1, 1) == -1) {
            sem_fail();
    }

}

void dest_semaphores() {
    sem_destroy(santa_sem);
    sem_destroy(elf_sem);
    sem_destroy(rndr_sem);
    sem_destroy(mutex);
    munmap(santa_sem, sizeof(sem_t));
    munmap(elf_sem, sizeof(sem_t));
    munmap(rndr_sem, sizeof(sem_t));
    munmap(mutex, sizeof(sem_t));
}

void santa(count_t *counters) {
    sem_wait(mutex);
    fprintf(out,"%d: Santa: going to sleep\n", counters->process_id++);
    fflush(NULL);
    sem_post(mutex);

    while(counters->workshop_not_closed) {
        sem_wait(santa_sem);

        if (counters->elves_to_help >= 3) {
            sem_wait(mutex);
            fprintf(out,"%d: Santa: helping elves\n", counters->process_id++);
            fflush(NULL);
            sem_post(elf_sem);
            sem_post(elf_sem);
            sem_post(elf_sem);
            counters->elves_to_help -= 3;
            sem_post(mutex);
            sem_wait(mutex);
            fprintf(out,"%d: Santa: going to sleep\n", counters->process_id++);
            fflush(NULL);
            sem_post(mutex);
        }

        if (counters->rndrs_back == nr) {
            sem_wait(mutex);
            fprintf(out,"%d: Santa: closing workshop\n", counters->process_id++);
            fflush(NULL);
            counters->workshop_not_closed = false;
            for (int i = 0; i < nr; i++) {
                sem_post(rndr_sem);
            }
            sem_post(mutex);
        }
    }

    while(true) {
        sem_wait(santa_sem);
        if (counters->hitched_rndrs == nr){
            sem_wait(mutex);
            fprintf(out,"%d: Santa: Christmas started\n", counters->process_id++);
            fflush(NULL);
            while (counters->elves_to_help != 0)
                sem_post(elf_sem);
            sem_post(mutex);
            return;
        }
    }
}

void elf(count_t *counters) {
    sem_wait(mutex);
    fprintf(out,"%d: Elf %d: started\n",counters->process_id++, elf_id);
    fflush(NULL);
    sem_post(mutex);

    while(true) {
        usleep(wait_usleep(1, te));
        sem_wait(mutex);
        fprintf(out,"%d: Elf %d: need help\n",counters->process_id++, elf_id);
        fflush(NULL);
        sem_post(mutex);
        sem_wait(mutex);
        if (!counters->workshop_not_closed) {
            fprintf(out,"%d: Elf %d: taking holidays\n",counters->process_id++, elf_id);
            fflush(NULL);
            sem_post(mutex);
            return;
        }
        else if (++counters->elves_to_help >= 3 && counters->workshop_not_closed)
            sem_post(santa_sem);

        sem_post(mutex);
        sem_wait(elf_sem);
        if (!counters->workshop_not_closed) {
            fprintf(out,"%d: Elf %d: taking holidays\n",counters->process_id++, elf_id);
            fflush(NULL);
            sem_post(mutex);
            return;
        }
        fprintf(out,"%d: Elf %d: get help\n",counters->process_id++, elf_id);
        fflush(NULL);
    }
}

void reindeer(count_t *counters) {
    sem_wait(mutex);
    fprintf(out,"%d: RD %d: started\n",counters->process_id++, rndr_id);
    fflush(NULL);
    sem_post(mutex);

    usleep(wait_usleep(tr / 2, tr));

    sem_wait(mutex);
    fprintf(out,"%d: RD %d: return home\n",counters->process_id++, rndr_id);
    fflush(NULL);
    if (++counters->rndrs_back == nr) {
        sem_post(santa_sem);
    }
    sem_post(mutex);

    sem_wait(rndr_sem);
    sem_wait(mutex);
    fprintf(out,"%d: RD %d: get hitched\n",counters->process_id++, rndr_id);
    fflush(NULL);
    if (++counters->hitched_rndrs == nr)
        sem_post(santa_sem);
    sem_post(mutex);
    return;
}

int main(int argc, char** argv) {

    /*=== zpracovani vstupnich argumentu ===*/

    //4 argumenty => NE (0,1000) NR (0,20) === TE <0,1000> TR <0,1000> ms
    if (argc != 5) {
        fprintf(stderr, "nespravny pocet argumentu (4)\n");
        return 1;
    }
    ne = atoi(argv[1]);
    nr = atoi(argv[2]);
    te = atoi(argv[3]);
    tr = atoi(argv[4]);
    if ((ne <= 0 || ne >= 1000) || (nr <= 0 || nr >= 20) || (te < 0 || te > 1000) || (tr < 0 || tr > 1000)) {
        fprintf(stderr, "nespravny rozsah argumentu:\n");
        fprintf(stderr, "ne:      0 < pocet elfu < 1000\n");
        fprintf(stderr, "nr:      0 < pocet sobu < 20\n");
        fprintf(stderr, "te:      0 < max doba skritkovy prace v ms < 1000\n");
        fprintf(stderr, "tr:      0 < max doba sobovy dovolene v ms < 1000\n");
        return 1;
    }

    /*=== otevreni vystupniho souboru ===*/

    out = fopen("proj2.out", "w");
    if(out == NULL) {
        fprintf(stderr, "nepodarilo se otevrit soubor pro zapis\n");
        return 1;
    }

    /*=== inicializace sdilene pameti ===*/
    void *counters = NULL;
    if((counters = mmap(NULL, sizeof(count_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED){
        fprintf(stderr, "chyba pri inicializaci sdilene pameti \n");
        return 1;
    }

    ((count_t *)counters)->process_id = 1;
    ((count_t *)counters)->hitched_rndrs = 0;
    ((count_t *)counters)->elves_to_help = 0;
    ((count_t *)counters)->workshop_not_closed = true;
    ((count_t *)counters)->rndrs_back = 0;

    /*=== inicializace semaforu ===*/
    init_semaphores();

    /*=== vytvoreni procesu santa, elfove, sobi ===*/
    for(int i=0; i < ne + nr + 1; i++) {
        switch(fork()) {
            case 0:
                if(i==0) { // santa
                    santa_id = i;
                    santa(((count_t *)counters));
                    return 0;
                } else { // elfove nebo sobi
                    if (i < ne + 1) {
                        elf_id = i;
                        elf(((count_t *)counters));
                    } else {
                        rndr_id = i - ne;
                        reindeer(((count_t *)counters));
                    }
                    return 0;
                }

            case -1:
                fprintf(stderr, "chyba pri vytvareni procesu\n");
                return 2;

            default:
                break;
        }
    }

    /*=== cekani na ukonceni vsech procesu ===*/
    for(int i = 0; i < ne + 1; i++) {
        wait(NULL);
    }

    /*=== uklizeni pameti, semaforu ===*/
    munmap(counters, sizeof(count_t));
    dest_semaphores();
    fclose(out);
    return 0;
}

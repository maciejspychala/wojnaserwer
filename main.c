#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include "structs.h"
#include <sys/sem.h>
#include <string.h>
#include <signal.h>

static struct sembuf buf;

int clientsId[2] = {};
int semid;
int howManyClients = 0;
int shareKey = 1324231;
struct Data *data;
int zyje = 1;
void podnies(int semnum) {
    buf.sem_num = semnum;
    buf.sem_op = 1;
    buf.sem_flg = 0;
    if (semop(semid, &buf, 1) == -1) {
        perror("Podnoszenie semafora");
        exit(1);
    }

}

void opusc(int semnum) {
    buf.sem_num = semnum;
    buf.sem_op = -1;
    buf.sem_flg = 0;
    if (semop(semid, &buf, 1) == -1) {
        perror("Opuszczenie semafora");
        exit(1);

    }
}

void sendData() {
    int i = 0;
    opusc(1);
    for (; i < 2; i++) {
        if (msgsnd(clientsId[i], &data[i], sizeof(data[i]) - sizeof(long), 0) < 0) {
            perror("wysylanie danych");
        }
    }
    podnies(1);
}

void sendInfo(int i, char *info) {
    strcpy(data[i].info, info);
    sendData();
    strcpy(data[i].info, "");
}

void buduj(struct Build build, int i) {
    while (build.light > 0) {
        sleep(2);
        build.light--;
        opusc(0);
        ++data[i].light;
        podnies(0);
        sendData();
    }
    while (build.heavy > 0) {
        sleep(3);
        build.heavy--;
        opusc(0);
        ++data[i].heavy;
        podnies(0);
        sendData();
    }
    while (build.cavalry > 0) {
        sleep(5);
        build.cavalry--;
        opusc(0);
        ++data[i].cavalry;
        podnies(0);
        sendData();
    }
    while (build.workers > 0) {
        sleep(2);
        build.workers--;
        opusc(0);
        ++data[i].workers;
        podnies(0);
        sendData();
    }
}

double liczAtak(double light, double heavy, double cavalry) {
    return (light + heavy * 1.5 + cavalry * 3.5);
}

double liczObrone(double light, double heavy, double cavalry) {
    return (light * 1.2 + heavy * 3 + cavalry * 1.2);
}

void atakuj(struct Attack attack, int i) {
    int atakujacy = i;
    int broniacy = (i + 1) % 2;
    sleep(5);
    double atak[2];
    double obrona[2];
    int atakUdany = 0;
    opusc(0);
    atak[atakujacy] = liczAtak(attack.light, attack.heavy, attack.cavalry);
    atak[broniacy] = liczAtak(data[broniacy].light, data[broniacy].heavy, data[broniacy].cavalry);
    obrona[atakujacy] = liczObrone(attack.light, attack.heavy, attack.cavalry);
    obrona[broniacy] = liczObrone(data[broniacy].light, data[broniacy].heavy, data[broniacy].cavalry);
    struct Attack straty[2];
    printf("atakujacy %d\nbroniacy%d\n", atakujacy, broniacy);
    printf("sila atakujacego %f obrona %f\nsila broniacego %f obrona %f\n",
           atak[atakujacy], obrona[atakujacy], atak[broniacy], obrona[broniacy]);
    if (atak[atakujacy] > obrona[broniacy]) {
        printf("atak udany\n");
        atakUdany = 1;
        ++data[atakujacy].points;
        straty[broniacy].light = data[broniacy].light;
        straty[broniacy].heavy = data[broniacy].heavy;
        straty[broniacy].cavalry = data[broniacy].cavalry;
        data[broniacy].light = 0;
        data[broniacy].heavy = 0;
        data[broniacy].cavalry = 0;
    } else {
        printf("atak nieudany\n");
        straty[broniacy].light = (int) (data[broniacy].light * (atak[atakujacy] / obrona[broniacy]));
        straty[broniacy].heavy = (int) (data[broniacy].heavy * (atak[atakujacy] / obrona[broniacy]));
        straty[broniacy].cavalry = (int) (data[broniacy].cavalry * (atak[atakujacy] / obrona[broniacy]));
        data[broniacy].light -= straty[broniacy].light;
        data[broniacy].heavy -= straty[broniacy].heavy;
        data[broniacy].cavalry -= straty[broniacy].cavalry;
    }

    if (atak[broniacy] > obrona[atakujacy]) {
        printf("obrona udana\n");
        straty[atakujacy].light = attack.light;
        straty[atakujacy].heavy = attack.heavy;
        straty[atakujacy].cavalry = attack.cavalry;
    } else {
        printf("obrona nieudana\n");
        straty[atakujacy].light = (int) (attack.light * (atak[broniacy] / obrona[atakujacy]));
        straty[atakujacy].heavy = (int) (attack.heavy * (atak[broniacy] / obrona[atakujacy]));
        straty[atakujacy].cavalry = (int) (attack.cavalry * (atak[broniacy] / obrona[atakujacy]));
        data[atakujacy].light += (attack.light - straty[atakujacy].light);
        data[atakujacy].heavy += (attack.heavy - straty[atakujacy].heavy);
        data[atakujacy].cavalry += (attack.cavalry - straty[atakujacy].cavalry);
    }

    char infoAtak[120];
    char infoDef[120];

    if (atakUdany) {
        sprintf(infoAtak, "Atak udany. Stracono %d lekkiej, %d ciezkiej i %d jazdy.",
                straty[atakujacy].light, straty[atakujacy].heavy, straty[atakujacy].cavalry);
        sprintf(infoDef, "Pokonano Cie. Stracono %d lekkiej, %d ciezkiej i %d jazdy.",
                straty[broniacy].light, straty[broniacy].heavy, straty[broniacy].cavalry);
    } else {
        sprintf(infoAtak, "Atak się nie powiódł. Stracono %d lekkiej, %d ciezkiej i %d jazdy.",
                straty[atakujacy].light, straty[atakujacy].heavy, straty[atakujacy].cavalry);
        sprintf(infoDef, "Obrona udana. Stracono %d lekkiej, %d ciezkiej i %d jazdy.",
                straty[broniacy].light, straty[broniacy].heavy, straty[broniacy].cavalry);
    }
    sendInfo(atakujacy, infoAtak);
    sendInfo(broniacy, infoDef);
    if (data[atakujacy].points >= 5) {
        data[atakujacy].end = 1;
        sendInfo(atakujacy, "Gratulacje, wygrales!");
        data[broniacy].end = 1;
        sendInfo(broniacy, "Niestety, przegrales.");

    }
    podnies(0);

}

void czyAtakuja() {
    struct Attack attack[2];
    int i = 0;
    if (fork() == 0)
        i = 1;
    while (zyje) {
        int moznaAtakowac = 0;
        msgrcv(clientsId[i], &attack[i], sizeof(attack[i]) - sizeof(long), 3, 0);
        printf("klient %d chce atakowac %d %d %d \n",
               i + 1, attack[i].light, attack[i].heavy, attack[i].cavalry);
        opusc(0);
        if (attack[i].light <= data[i].light &&
            attack[i].heavy <= data[i].heavy &&
            attack[i].cavalry <= data[i].cavalry) {
            char info[120];
            sprintf(info, "atakujemy: lekka %d, ciezka %d, jazda %d",
                    attack[i].light, attack[i].heavy, attack[i].cavalry);
            data[i].light -= attack[i].light;
            data[i].heavy -= attack[i].heavy;
            data[i].cavalry -= attack[i].cavalry;
            sendInfo(i, info);
            moznaAtakowac = 1;
        } else {
            sendInfo(i, "Nie masz wystarczajaco jednostek");
        }
        podnies(0);
        if (moznaAtakowac && fork() == 0) {
            atakuj(attack[i], i);
            exit(0);
        }
    }
}

void czyChcaBudowac() {
    struct Build build[2];
    int i = 0;
    if (fork() == 0)
        i = 1;
    while (zyje) {
        msgrcv(clientsId[i], &build[i], sizeof(build[i]) - sizeof(long), 2, 0);
        printf("klient %d chce budowac %d %d %d %d \n",
               i + 1, build[i].light, build[i].heavy, build[i].cavalry, build[i].workers);

        int cena = build[i].light * 100 + build[i].heavy * 250 +
                   build[i].cavalry * 550 + build[i].workers * 150;
        int moznaBudowac = 0;
        opusc(0);
        if (cena <= data[i].resources) {
            char info[120];
            sprintf(info, "budujemy: lekka %d, ciezka %d, jazda %d, robotnicy %d za %d talarow",
                    build[i].light, build[i].heavy, build[i].cavalry, build[i].workers, cena);
            data[i].resources -= cena;
            sendInfo(i, info);
            moznaBudowac = 1;
        } else {
            sendInfo(i, "Nie masz wystarczajaco srodkow");
        }
        podnies(0);
        if (moznaBudowac && fork() == 0) {
            opusc(2 + i);
            buduj(build[i], i);
            podnies(2 + i);
        }

    }
}


void setConnections(int kolejkaId) {
    struct Init init;

    while (msgrcv(kolejkaId, &init, sizeof(init.nextMsg), 2, IPC_NOWAIT) > -1);
    while (msgrcv(kolejkaId, &init, sizeof(init.nextMsg), 1, IPC_NOWAIT) > -1);
    init.mtype = 1;
    while (howManyClients != 2) {
        while ((clientsId[howManyClients] = msgget(shareKey++, IPC_CREAT | IPC_EXCL | 0640)) < 0);
        init.nextMsg = --shareKey;
        shareKey++;
        printf("stawiam kolejke id = %d\n", init.nextMsg);
        msgsnd(kolejkaId, &init, sizeof(init.nextMsg), 0);
        ++howManyClients;
    }

    msgrcv(kolejkaId, &init, sizeof(init.nextMsg), 2, 0);
    printf("polaczyl sie %d klient\n", 1);
    msgrcv(kolejkaId, &init, sizeof(init.nextMsg), 2, 0);
    printf("polaczyl sie %d klient\n", 2);
}

void setDataForStart() {
    int i = 0;
    for (; i < 2; i++) {
        data[i].mtype = 0;
        data[i].light = 0;
        data[i].heavy = 0;
        data[i].cavalry = 0;
        data[i].workers = 0;
        data[i].points = 0;
        data[i].resources = 300;
        strcpy(data[i].info, "");
        data[i].end = 0;
    }
}

void jednostkiCoSekunde() {
    while (zyje) {
        opusc(0);
        data[0].resources += 50 + 5 * data[0].workers;
        data[1].resources += 50 + 5 * data[1].workers;
        podnies(0);
        sendData();
        sleep(1);
    }
}

void setSem() {
    semid = semget(322421, 4, IPC_CREAT | IPC_EXCL | 0640);

    if (semid == -1) {
        semid = semget(322421, 4, 0600);
        if (semid == -1) {
            perror("Utworzenie tablicy semaforow");
            exit(1);
        }
    }

    if (semctl(semid, 0, SETVAL, (int) 1) == -1) {
        perror("Nadanie wartosci semaforowi 0");
        exit(1);
    }
    if (semctl(semid, 1, SETVAL, (int) 1) == -1) {
        perror("Nadanie wartosci semaforowi 1");
        exit(1);
    }
    if (semctl(semid, 2, SETVAL, (int) 1) == -1) {
        perror("Nadanie wartosci semaforowi 2");
        exit(1);
    }
    if (semctl(semid, 3, SETVAL, (int) 1) == -1) {
        perror("Nadanie wartosci semaforowi 3");
        exit(1);
    }
}

void czyZyja() {
    int i = 0;
    int howMany = 0;
    int recive;
    if (fork() == 0) {
        i = 1;
    }
    struct Alive alive;
    while (zyje) {
        recive = msgrcv(clientsId[i], &alive, sizeof(alive) - sizeof(long), 4, IPC_NOWAIT);
        if (recive < 0) {
            ++howMany;
            printf("ktos nie zyje %d\n", i + 1);
        }
        else {
            howMany = 0;
        }
        if (howMany > 2) {
            system("clear");
            printf("utracono polaczenie z klientem %d\n", i + 1);
            data[(i + 1) % 2].points = 5;
            data[(i + 1) % 2].end = 1;
            sendInfo((i + 1) % 2, "Drugi Klient rozlaczyl sie");
            zyje = 0;
            exit(0);
        }
        sleep(2);
    }

}

void jaZyje() {
    struct Alive alive;
    alive.mtype = 5;
    while (zyje) {
        msgsnd(clientsId[0], &alive, sizeof(alive) - sizeof(long), 0);
        msgsnd(clientsId[1], &alive, sizeof(alive) - sizeof(long), 0);
        sleep(2);
    }
    exit(0);
}

int main() {
    int key = 15071410;
    int kolejkaInitId = msgget(key, IPC_CREAT | 0640);
    setSem();

    int p = shmget(clientsId[0], 2 * sizeof(struct Data), IPC_CREAT | 0640);
    data = (struct Data *) shmat(p, NULL, 0);

    if (kolejkaInitId == -1) {
        perror("Utworzenie kolejki komunikatow");
        exit(1);
    }

    setConnections(kolejkaInitId);
    setDataForStart();

    if (fork() == 0) {
        jednostkiCoSekunde();
    } else if (fork() == 0) {
        czyChcaBudowac();
    } else if (fork() == 0) {
        czyAtakuja();
    } else if (fork() == 0) {
        czyZyja();
    } else {
        jaZyje();
    }
    kill(0, SIGKILL);
    exit(0);
}
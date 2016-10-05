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
int serverAlive = 1;

void pickUpSem(unsigned short semnum) {
    buf.sem_num = semnum;
    buf.sem_op = 1;
    buf.sem_flg = 0;
    if (semop(semid, &buf, 1) == -1) {
        perror("Podnoszenie semafora");
        exit(1);
    }

}

void lowerSem(unsigned short semnum) {
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
    lowerSem(1);
    for (; i < 2; i++) {
        if (msgsnd(clientsId[i], &data[i], sizeof(data[i]) - sizeof(long), 0) < 0) {
            perror("wysylanie danych");
        }
    }
    pickUpSem(1);
}

void sendInfo(int i, char *info) {
    strcpy(data[i].info, info);
    sendData();
    strcpy(data[i].info, "");
}

void startBuilding(struct Build build, int i) {
    while (build.light > 0) {
        sleep(2);
        build.light--;
        lowerSem(0);
        ++data[i].light;
        pickUpSem(0);
        sendData();
    }
    while (build.heavy > 0) {
        sleep(3);
        build.heavy--;
        lowerSem(0);
        ++data[i].heavy;
        pickUpSem(0);
        sendData();
    }
    while (build.cavalry > 0) {
        sleep(5);
        build.cavalry--;
        lowerSem(0);
        ++data[i].cavalry;
        pickUpSem(0);
        sendData();
    }
    while (build.workers > 0) {
        sleep(2);
        build.workers--;
        lowerSem(0);
        ++data[i].workers;
        pickUpSem(0);
        sendData();
    }
}

double countAttackPower(double light, double heavy, double cavalry) {
    return (light + heavy * 1.5 + cavalry * 3.5);
}

double countDefPower(double light, double heavy, double cavalry) {
    return (light * 1.2 + heavy * 3 + cavalry * 1.2);
}

void startAttack(struct Attack attack, int i) {
    int attacker = i;
    int defender = (i + 1) % 2;
    sleep(5);
    double atkPower[2];
    double defPower[2];
    int attackSuccessful = 0;
    lowerSem(0);
    atkPower[attacker] = countAttackPower(attack.light, attack.heavy, attack.cavalry);
    atkPower[defender] = countAttackPower(data[defender].light, data[defender].heavy, data[defender].cavalry);
    defPower[attacker] = countDefPower(attack.light, attack.heavy, attack.cavalry);
    defPower[defender] = countDefPower(data[defender].light, data[defender].heavy, data[defender].cavalry);
    struct Attack loss[2];
    printf("atakujacy %d\nbroniacy%d\n", attacker, defender);
    printf("sila atakujacego %f obrona %f\nsila broniacego %f obrona %f\n",
           atkPower[attacker], defPower[attacker], atkPower[defender], defPower[defender]);
    if (atkPower[attacker] > defPower[defender]) {
        printf("atak udany\n");
        attackSuccessful = 1;
        ++data[attacker].points;
        loss[defender].light = data[defender].light;
        loss[defender].heavy = data[defender].heavy;
        loss[defender].cavalry = data[defender].cavalry;
        data[defender].light = 0;
        data[defender].heavy = 0;
        data[defender].cavalry = 0;
    } else {
        printf("atak nieudany\n");
        loss[defender].light = (int) (data[defender].light * (atkPower[attacker] / defPower[defender]));
        loss[defender].heavy = (int) (data[defender].heavy * (atkPower[attacker] / defPower[defender]));
        loss[defender].cavalry = (int) (data[defender].cavalry * (atkPower[attacker] / defPower[defender]));
        data[defender].light -= loss[defender].light;
        data[defender].heavy -= loss[defender].heavy;
        data[defender].cavalry -= loss[defender].cavalry;
    }

    if (atkPower[defender] > defPower[attacker]) {
        printf("obrona udana\n");
        loss[attacker].light = attack.light;
        loss[attacker].heavy = attack.heavy;
        loss[attacker].cavalry = attack.cavalry;
    } else {
        printf("obrona nieudana\n");
        loss[attacker].light = (int) (attack.light * (atkPower[defender] / defPower[attacker]));
        loss[attacker].heavy = (int) (attack.heavy * (atkPower[defender] / defPower[attacker]));
        loss[attacker].cavalry = (int) (attack.cavalry * (atkPower[defender] / defPower[attacker]));
        data[attacker].light += (attack.light - loss[attacker].light);
        data[attacker].heavy += (attack.heavy - loss[attacker].heavy);
        data[attacker].cavalry += (attack.cavalry - loss[attacker].cavalry);
    }

    char infoAtk[120];
    char infoDef[120];

    if (attackSuccessful) {
        sprintf(infoAtk, "Atak udany. Stracono %d lekkiej, %d ciezkiej i %d jazdy.",
                loss[attacker].light, loss[attacker].heavy, loss[attacker].cavalry);
        sprintf(infoDef, "Pokonano Cie. Stracono %d lekkiej, %d ciezkiej i %d jazdy.",
                loss[defender].light, loss[defender].heavy, loss[defender].cavalry);
    } else {
        sprintf(infoAtk, "Atak się nie powiódł. Stracono %d lekkiej, %d ciezkiej i %d jazdy.",
                loss[attacker].light, loss[attacker].heavy, loss[attacker].cavalry);
        sprintf(infoDef, "Obrona udana. Stracono %d lekkiej, %d ciezkiej i %d jazdy.",
                loss[defender].light, loss[defender].heavy, loss[defender].cavalry);
    }
    sendInfo(attacker, infoAtk);
    sendInfo(defender, infoDef);
    if (data[attacker].points >= 5) {
        data[attacker].end = 1;
        sendInfo(attacker, "Gratulacje, wygrales!");
        data[defender].end = 1;
        sendInfo(defender, "Niestety, przegrales.");

    }
    pickUpSem(0);

}

void ifSomeoneAttack() {
    struct Attack attack[2];
    int i = 0;
    if (fork() == 0)
        i = 1;
    while (serverAlive) {
        int canAttack = 0;
        msgrcv(clientsId[i], &attack[i], sizeof(attack[i]) - sizeof(long), 3, 0);
        printf("klient %d chce atakowac %d %d %d \n",
               i + 1, attack[i].light, attack[i].heavy, attack[i].cavalry);
        lowerSem(0);
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
            canAttack = 1;
        } else {
            sendInfo(i, "Nie masz wystarczajaco jednostek");
        }
        pickUpSem(0);
        if (canAttack && fork() == 0) {
            startAttack(attack[i], i);
            exit(0);
        }
    }
}

void ifWantBuild() {
    struct Build build[2];
    int i = 0;
    if (fork() == 0)
        i = 1;
    while (serverAlive) {
        msgrcv(clientsId[i], &build[i], sizeof(build[i]) - sizeof(long), 2, 0);
        printf("klient %d chce budowac %d %d %d %d \n",
               i + 1, build[i].light, build[i].heavy, build[i].cavalry, build[i].workers);

        int cost = build[i].light * 100 + build[i].heavy * 250 +
                   build[i].cavalry * 550 + build[i].workers * 150;
        int canBuild = 0;
        lowerSem(0);
        if (cost <= data[i].resources) {
            char info[120];
            sprintf(info, "budujemy: lekka %d, ciezka %d, jazda %d, robotnicy %d za %d talarow",
                    build[i].light, build[i].heavy, build[i].cavalry, build[i].workers, cost);
            data[i].resources -= cost;
            sendInfo(i, info);
            canBuild = 1;
        } else {
            sendInfo(i, "Nie masz wystarczajaco srodkow");
        }
        pickUpSem(0);
        if (canBuild && fork() == 0) {
            lowerSem(2 + i);
            startBuilding(build[i], i);
            pickUpSem(2 + i);
        }

    }
}


void setConnections(int msgID) {
    struct Init init;

    while (msgrcv(msgID, &init, sizeof(init.nextMsg), 2, IPC_NOWAIT) > -1);
    while (msgrcv(msgID, &init, sizeof(init.nextMsg), 1, IPC_NOWAIT) > -1);
    init.mtype = 1;
    while (howManyClients != 2) {
        while ((clientsId[howManyClients] = msgget(shareKey++, IPC_CREAT | IPC_EXCL | 0640)) < 0);
        init.nextMsg = --shareKey;
        shareKey++;
        printf("stawiam kolejke id = %d\n", init.nextMsg);
        msgsnd(msgID, &init, sizeof(init.nextMsg), 0);
        ++howManyClients;
    }

    msgrcv(msgID, &init, sizeof(init.nextMsg), 2, 0);
    printf("polaczyl sie %d klient\n", 1);
    msgrcv(msgID, &init, sizeof(init.nextMsg), 2, 0);
    printf("polaczyl sie %d klient\n", 2);
}

void setDataForStart() {
    int i = 0;
    for (; i < 2; i++) {
        data[i].mtype = 1;
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

void unitsPerSec() {
    while (serverAlive) {
        lowerSem(0);
        data[0].resources += 50 + 5 * data[0].workers;
        data[1].resources += 50 + 5 * data[1].workers;
        pickUpSem(0);
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

void ifClientsAlive() {
    int i = 0;
    int howMany = 0;
    int receive;
    if (fork() == 0) {
        i = 1;
    }
    struct Alive alive;
    while (serverAlive) {
        receive = msgrcv(clientsId[i], &alive, sizeof(alive) - sizeof(long), 4, IPC_NOWAIT);
        if (receive < 0) {
            ++howMany;
            printf("ktos nie zyje %d\n", i + 1);
        } else {
            howMany = 0;
        }
        if (howMany > 2) {
            system("clear");
            printf("utracono polaczenie z klientem %d\n", i + 1);
            data[(i + 1) % 2].points = 5;
            data[(i + 1) % 2].end = 1;
            sendInfo((i + 1) % 2, "Drugi Klient rozlaczyl sie");
            serverAlive = 0;
            exit(0);
        }
        sleep(2);
    }

}

void pingClients() {
    struct Alive alive;
    alive.mtype = 5;
    while (serverAlive) {
        msgsnd(clientsId[0], &alive, sizeof(alive) - sizeof(long), 0);
        msgsnd(clientsId[1], &alive, sizeof(alive) - sizeof(long), 0);
        sleep(2);
    }
    exit(0);
}

int main() {
    int key = 15071410;
    int initMsgID = msgget(key, IPC_CREAT | 0640);
    setSem();

    int p = shmget(clientsId[0], 2 * sizeof(struct Data), IPC_CREAT | 0640);
    data = (struct Data *) shmat(p, NULL, 0);

    if (initMsgID == -1) {
        perror("Utworzenie kolejki komunikatow");
        exit(1);
    }

    setConnections(initMsgID);
    setDataForStart();

    if (fork() == 0) {
        unitsPerSec();
    } else if (fork() == 0) {
        ifWantBuild();
    } else if (fork() == 0) {
        ifSomeoneAttack();
    } else if (fork() == 0) {
        ifClientsAlive();
    } else {
        pingClients();
    }
    kill(0, SIGKILL);
    exit(0);
}
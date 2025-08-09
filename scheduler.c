#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <errno.h>
#include <time.h>

#define MAX_DOCKS 30
#define MAX_CRANES 25
#define MAX_CARGO_COUNT 200
#define MAX_NEW_REQUESTS 100
#define MAX_AUTH_STRING_LEN 100
#define MAX_SHIPS 2000
#define MAX_SOLVERS 8

// --- Assignment-provided structures ---

typedef struct {
    long mtype;
    int timestep;
    int shipId;
    int direction;
    int dockId;
    int cargoId;
    int isFinished;
    union {
        int numShipRequests;
        int craneId;
    };
} MessageStruct;

typedef struct {
    int shipId;
    int timestep;
    int category;
    int direction;
    int emergency;
    int waitingTime;
    int numCargo;
    int cargo[MAX_CARGO_COUNT];
} ShipRequest;

typedef struct {
    char authStrings[MAX_DOCKS][MAX_AUTH_STRING_LEN];
    ShipRequest newShipRequests[MAX_NEW_REQUESTS];
} MainSharedMemory;

typedef struct {
    long mtype;
    int dockId;
    char authStringGuess[MAX_AUTH_STRING_LEN];
} SolverRequest;

typedef struct {
    long mtype;
    int guessIsCorrect;
} SolverResponse;

// --- Local structures ---

typedef struct {
    int category;
    int numCranes;
    int craneCapacities[MAX_CRANES];
    bool isOccupied;
    int lastUndockTimestep;
} Dock;

typedef struct {
    int shipId;
    int direction;
    int category;
    int emergency;
    int arrivalTime;
    int waitingTime;
    int numCargo;
    int cargo[MAX_CARGO_COUNT];
    int assignedDock;
    int dockedTime;
    int cargoProcessed;
    int lastCargoTimestep;
    bool isDocked;
    bool isServiced;
    bool isActive;
    bool authDone;
} Ship;

// --- Globals ---
int mainMsgQueue;
int solverQueues[MAX_SOLVERS];
int numSolvers;
MainSharedMemory *sharedMem;
Dock docks[MAX_DOCKS];
int numDocks;
Ship ships[MAX_SHIPS];
int totalShips = 0;
int currentTimestep = 0;

// --- Helper Functions ---

// Read all IPC and dock info from input.txt
int setupIPC(const char *inputFile) {
    FILE *fp = fopen(inputFile, "r");
    if (!fp) return -1;

    key_t shmKey, mainQueueKey;
    fscanf(fp, "%d", &shmKey);
    fscanf(fp, "%d", &mainQueueKey);
    fscanf(fp, "%d", &numSolvers);

    for (int i = 0; i < numSolvers; i++) {
        key_t solverKey;
        fscanf(fp, "%d", &solverKey);
        solverQueues[i] = msgget(solverKey, 0666);
    }
    mainMsgQueue = msgget(mainQueueKey, 0666);
    sharedMem = (MainSharedMemory *)shmat(shmget(shmKey, 0, 0666), NULL, 0);

    fscanf(fp, "%d", &numDocks);
    for (int i = 0; i < numDocks; i++) {
        fscanf(fp, "%d", &docks[i].category);
        docks[i].numCranes = docks[i].category;
        for (int j = 0; j < docks[i].numCranes; j++)
            fscanf(fp, "%d", &docks[i].craneCapacities[j]);
        docks[i].isOccupied = false;
        docks[i].lastUndockTimestep = 0;
    }
    fclose(fp);
    return 0;
}

// Try to assign ship to any suitable dock (greedy, as per assignment)
int assignDock(Ship *ship) {
    for (int d = 0; d < numDocks; d++) {
        if (!docks[d].isOccupied && docks[d].category >= ship->category && docks[d].lastUndockTimestep != currentTimestep) {
            docks[d].isOccupied = true;
            ship->assignedDock = d;
            ship->dockedTime = currentTimestep;
            ship->isDocked = true;
            MessageStruct dockMsg = {.mtype = 2, .dockId = d, .shipId = ship->shipId, .direction = ship->direction};
            msgsnd(mainMsgQueue, &dockMsg, sizeof(dockMsg) - sizeof(long), 0);
            return 1;
        }
    }
    return 0;
}

// Process all possible cargo for a ship in this timestep (one per crane)
void processCargo(Ship *ship) {
    if (!ship->isDocked || ship->isServiced || currentTimestep <= ship->dockedTime) return;
    bool craneUsed[MAX_CRANES] = {false};
    int dockId = ship->assignedDock;
    for (int i = ship->cargoProcessed; i < ship->numCargo; i++) {
        int craneId = -1;
        for (int c = 0; c < docks[dockId].numCranes; c++) {
            if (!craneUsed[c] && docks[dockId].craneCapacities[c] >= ship->cargo[i]) {
                craneId = c; break;
            }
        }
        if (craneId == -1) break;
        craneUsed[craneId] = true;
        MessageStruct cargoMsg = {.mtype = 4, .dockId = dockId, .shipId = ship->shipId, .direction = ship->direction, .cargoId = i, .craneId = craneId};
        msgsnd(mainMsgQueue, &cargoMsg, sizeof(cargoMsg) - sizeof(long), 0);
        ship->cargoProcessed++;
        ship->lastCargoTimestep = currentTimestep;
    }
}

// Generate the next possible authentication string (systematic, not random)
void genAuthString(char *buf, int length, int n) {
    // There are 5 options for first/last char, 6 for each middle char
    // n is the nth string in lexicographic order
    static const char *chars = "56789.";
    int total = 1;
    for (int i = 0; i < length; i++) total *= (i == 0 || i == length-1) ? 5 : 6;
    n = n % total;
    for (int i = 0; i < length; i++) {
        int base = (i == 0 || i == length-1) ? 5 : 6;
        buf[i] = chars[n % base];
        n /= base;
    }
    buf[length] = 0;
}

// Try all possible auth strings until correct
bool authenticate(Ship *ship) {
    if (ship->authDone) return true;
    int dockId = ship->assignedDock;
    int length = ship->lastCargoTimestep - ship->dockedTime;
    if (length <= 0) return false;

    // Inform all solvers about the dock
    for (int i = 0; i < numSolvers; i++) {
        SolverRequest req = {.mtype = 1, .dockId = dockId};
        msgsnd(solverQueues[i], &req, sizeof(req) - sizeof(long), IPC_NOWAIT);
    }

    // Try all possible strings (lexicographically)
    char guess[MAX_AUTH_STRING_LEN];
    int maxTries = 1;
    for (int i = 0; i < length; i++) maxTries *= (i == 0 || i == length-1) ? 5 : 6;
    for (int t = 0; t < maxTries; t++) {
        genAuthString(guess, length, t);
        for (int s = 0; s < numSolvers; s++) {
            SolverRequest req = {.mtype = 2, .dockId = dockId};
            strncpy(req.authStringGuess, guess, MAX_AUTH_STRING_LEN);
            msgsnd(solverQueues[s], &req, sizeof(req) - sizeof(long), 0);
            SolverResponse resp;
            msgrcv(solverQueues[s], &resp, sizeof(resp) - sizeof(long), 3, 0);
            if (resp.guessIsCorrect == 1) {
                strncpy(sharedMem->authStrings[dockId], guess, MAX_AUTH_STRING_LEN);
                ship->authDone = true;
                return true;
            }
        }
    }
    return false;
}

// Undock a ship (after successful authentication)
void undockShip(Ship *ship) {
    int dockId = ship->assignedDock;
    MessageStruct undockMsg = {.mtype = 3, .dockId = dockId, .shipId = ship->shipId, .direction = ship->direction};
    msgsnd(mainMsgQueue, &undockMsg, sizeof(undockMsg) - sizeof(long), 0);
    docks[dockId].isOccupied = false;
    docks[dockId].lastUndockTimestep = currentTimestep;
    ship->isDocked = false;
    ship->isServiced = true;
}

// --- Main Scheduler Loop ---

int main(int argc, char *argv[]) {
    if (argc != 2) return 1;
    char inputFile[256];
    snprintf(inputFile, sizeof(inputFile), "testcase%s/input.txt", argv[1]);
    if (setupIPC(inputFile) != 0) return 1;
    srand(time(NULL));

    while (1) {
        MessageStruct msg;
        msgrcv(mainMsgQueue, &msg, sizeof(msg) - sizeof(long), 1, 0);
        currentTimestep = msg.timestep;
        if (msg.isFinished == 1) break;

        // 1. Add new ships
        for (int i = 0; i < msg.numShipRequests && totalShips < MAX_SHIPS; i++) {
            ShipRequest *req = &sharedMem->newShipRequests[i];
            Ship *ship = &ships[totalShips++];
            ship->shipId = req->shipId;
            ship->direction = req->direction;
            ship->category = req->category;
            ship->emergency = req->emergency;
            ship->arrivalTime = req->timestep;
            ship->waitingTime = req->waitingTime;
            ship->numCargo = req->numCargo;
            memcpy(ship->cargo, req->cargo, sizeof(int) * req->numCargo);
            ship->assignedDock = -1;
            ship->dockedTime = -1;
            ship->cargoProcessed = 0;
            ship->lastCargoTimestep = -1;
            ship->isDocked = false;
            ship->isServiced = false;
            ship->isActive = true;
            ship->authDone = false;
        }

        // 2. Emergency ships: assign as many as possible (maximum allocation)
        for (int i = 0; i < totalShips; i++) {
            Ship *s = &ships[i];
            if (s->direction == 1 && s->emergency == 1 && s->isActive && !s->isDocked && !s->isServiced)
                assignDock(s);
        }

        // 3. Regular and outgoing ships: assign greedily if possible
        for (int i = 0; i < totalShips; i++) {
            Ship *s = &ships[i];
            if (s->isActive && !s->isDocked && !s->isServiced) {
                bool canDock = true;
                if (s->direction == 1 && s->emergency == 0) {
                    canDock = (currentTimestep <= s->arrivalTime + s->waitingTime);
                    if (!canDock) s->isActive = false;
                }
                if (canDock) assignDock(s);
            }
        }

        // 4. Process cargo maximally for all docked ships
        for (int i = 0; i < totalShips; i++)
            processCargo(&ships[i]);

        // 5. Undock ships after all cargo and authentication
        for (int i = 0; i < totalShips; i++) {
            Ship *s = &ships[i];
            if (s->isDocked && !s->isServiced && s->cargoProcessed == s->numCargo && currentTimestep > s->lastCargoTimestep) {
                if (authenticate(s)) undockShip(s);
            }
        }

        // 6. Timestep update
        MessageStruct update = {.mtype = 5};
        msgsnd(mainMsgQueue, &update, sizeof(update) - sizeof(long), 0);
    }
    shmdt(sharedMem);
    return 0;
}

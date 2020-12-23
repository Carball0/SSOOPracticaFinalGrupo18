#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>

struct Paciente {
    int id;
    bool atendido;
    char tipo;  // Junior J, Medio M, Senior S
    bool serologia;
};

static int MAXPACIENTES = 15, ENFERMEROS = 3;
struct Paciente* pacientes;
int numPacientes;
pthread_t medico, estadistico;
pthread_t enfermeros[];

FILE* logFile;

void writeLogMessage(char *id, char *msg);

int main(int argc, char** argv) {

}

void writeLogMessage(char *id, char *msg) {
// Calculamos la hora actual
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);
// Escribimos en el log
    logFile = fopen("logFileName", "a");    //TODO
    fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
    fclose(logFile);
}
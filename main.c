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
struct Paciente pacientes[15];
int numPacientes;
pthread_t medico, estadistico;
pthread_t enfermeros[];

FILE* logFile;

void writeLogMessage(char *id, char *msg);
void nuevoPaciente(struct Paciente pacientes[],int &num_pacientes,int signal)

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
//CAMBIAR NOMBRE NEW PACIENTE Y CREAR EL HILO
//metodo que comprueba si hay sitio para la llegada de un nuevo paciente, y en ese caso lo crea
void nuevoPaciente(struct Paciente pacientes[],int &num_pacientes,int signal)
{
    for(int i = 0;i<15;i++){
        if(pacientes[i] == null){
            //si hay espacio, creamos un nuevo paciente y lo añadimos al array de pacientes
            pacientes new_paciente;
            pacientes[i] = new_paciente;
            //incrementamos el numero de pacientes
            num_pacientes++;
            new_paciente.id = num_pacientes;
            new_paciente.atendido = false;
            //13 = SIGPIPE; 16 = SIGUSR1; 17 = SIGUSR2
            switch(signal){
                case 13:
                    //paciente senior
                    new_paciente.tipo = 3;
                   break;
                case 16:
                    //paciente junior
                    new_paciente.tipo = 1;
                    break;
                case 17:
                    //paciente medio
                    new_paciente.tipo = 2;
                    break;
            }
            new_paciente.serologia = false;
        }
    }
}
//AÑADIR SINCRONIZACION
void accionesEstadistico(pthread_t estadistico)
{
//espera que le avisen de que hay un paciente en estudio (EXCLUSION MUTUA)
//escribe en el log el comienzo de actividad (EXCLUSION MUTUA)
writeLogMessage(estadistico_self,"Comienzo de actividad del estadistico.");
//calcula el tiempo de actividad
//termina la actividad y avisa al paciente (VARIABLES CONDICION)
//escribe en el log que finaliza la actividad (EXCLUSION MUTUA)
writeLogMessage(estadistico_self,"Fin de actividad del estadistico.");
//cambia paciente en estudio y vuelve a 1 (EXCLUSION MUTUA)
}
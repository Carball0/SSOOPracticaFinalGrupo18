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
int numPacientes, contEnfermero;
pthread_t medico, estadistico;
pthread_t enfermeros[];

FILE* logFile;

void writeLogMessage(char *id, char *msg);
void nuevoPaciente(struct Paciente pacientes[],int &num_pacientes,int signal);
void accionesPaciente(struct Paciente pacientes[]);
void accionesEnfermero(char tipo);
void accionesEstadistico(pthread_t estadistico);

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

void accionesEnfermero(char tipo) {
    srand(time(NULL));
    struct Paciente pacVacio;
    if(tipo=='J' || tipo=='M' || tipo=='S') {   // Tipo valido
        for(int i = 0; i<MAXPACIENTES; i++) {
            if(pacientes[i].tipo == tipo && !pacientes[i].atendido) {
                pacientes[i].atendido = true;
                int random = (rand()%100)+1;    //Random entre 0 y 100
                writeLogMessage("Enfermero", "Comienza la atención del paciente");
                if(random<=80) {    //To_do en regla
                    sleep((rand()%4)+1);
                    if(pacientes[i].serologia){
                        //TODO Se le manda a estudio
                    }
                    writeLogMessage("Enfermero", "Fin atención paciente exitosa");
                } else if(random>80 && random<= 90) {  //Mal identificados
                    sleep((rand()%5)+2);
                    if(pacientes[i].serologia){
                        // TODO Se le manda a estudio
                    }
                    writeLogMessage("Enfermero", "Fin atención paciente exitosa");
                } else if(random>90 && random<=100){   //Catarro o Gripe
                    sleep((rand()%5)+6);
                    writeLogMessage("Enfermero", "Fin atención paciente: Catarro o Gripe");
                    pacientes[i] = pacVacio;
                    //Abandonan consultorio sin reaccion ni estudio
                }
            }
            if(contEnfermero == 5) {
                writeLogMessage("Enfermero", "Descanso para el cafe");
                sleep(5);
            }
        }
    } else {    //Tipo invalido
        perror("Emfermero sin tipo valido");
        return;
    }
}

void accionesPaciente(struct Paciente pacientes[]){
    //Guardar en el log la hora de entrada.
    writeLogMessage(paciente_self,"Hora de entrada del paciente.");
    //Guardar en el log el tipo de solicitud.
    writeLogMessage(paciente_self,"Tipo de solicitud del paciente.");
    //Duerme 3 segundos
    sleep(3);
    //Comprueba si está siendo atendido.
    //Si no lo está, calculamos el comportamiento del paciente (si se va por cansarse 
    //de esperar, si se lo piensa mejor) o si se va al baño y pierde su turno.
    for(int i=0;i<15;i++){
        if(pacientes[i].atendido==false){
            int comportamientoPaciente=rand()% 100+1;
            //un 20 % de los pacientes se cansa de esperar y se va.
            if(comportamientoPaciente<=20){
                //Log que avisa de que se va por cansancio
                writeLogMessage(paciente_self,"El paciente se ha ido porque se ha cansado de esperar.");
                //codigo de cuando se va
            }else if(comportamientoPaciente>20&&comportamientoPaciente<=30){
                //Log que avisa de que se va porque se lo ha pensado mejor
                writeLogMessage(paciente_self,"El paciente se lo ha pensado mejor y se ha ido.");
                //codigo de cuando se lo piensa mejor y se va tambien.
            }else{
                //70% restante
                int comportamientoPacRestantes=rand()% 100+1;
                if(comportamientoPacRestantes<=5){
                    //Log que avisa de que ha perdido el turno por ir al baño
                    writeLogMessage(paciente_self,"El paciente ha ido al baño y ha perdido el turno.");
                    //Codigo de cuando se va al baño y pierde el turno.
                }else{
                    //Codigo de los pacientes que ni se van ni pierden turno.
                    //El paciente debe dormir 3 segundos y vuelve a 4.
                    sleep(4);
                }
            }
        }else{
            //Si está siendo atendido por el enfermer@ debemos esperar a que termine.
        }
        //Si no se va por gripe o catarro calcula si le da reacción
        int reaccionPaciente=rand()% 100+1;
        if(reaccionPaciente<=10){
            //Si le da cambia el valor de atendido a 4
            //Esperamos a que termine la atención
        }else{
            //Si no le da reacción calculamos si decide o no participar en el estudio serológico
            int participaEstudio=rand()% 100+1;
            if(participaEstudio<=25){
                //Si decide participar
                //Cambia el valor de la variable serológica
                    //Cambia el valor de paciente en estudio.
                    //Avisa al estadistico
                    //Guardamos el log en que está preparado para el estudio
                    //Se queda esperando a que digan que pueden marchar
                    //Guardamos el log en que deja el estudio
            }
        }
        //Libera su posición en cola de solicitudes y se va
        //Escribe en el log
        writeLogMessage(paciente_self,"El paciente ha terminado de vacunarse y se ha ido.");
        //Fin del hilo Paciente.
    }
}
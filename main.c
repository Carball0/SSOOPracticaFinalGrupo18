#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#define MAXPACIENTES 15
#define ENFERMEROS 3

struct Paciente {
    int id;
    int atendido;
    char tipo;  // Junior J, Medio M, Senior S
    bool serologia;
};

struct Enfermero {
    int id;
    char tipo;  // Junior J, Medio M, Senior S
};

struct Paciente pacientes[MAXPACIENTES];
struct Enfermero enfermeros[ENFERMEROS];
int numPacientes, contEnfermero;
pthread_t medico, estadistico;
pthread_t enfermero;
pthread_t hilo_paciente;
pthread_mutex_t mutexAccionesPaciente;
pthread_mutex_t mutex_hilos;
pthread_mutex_t mutex_enf;
FILE* logFile;
pthread_cond_t condicion;
pthread_cond_t condicionAccionesPyEnfermero;
pthread_cond_t condicionInfoMedicoyEnfermero;
pthread_cond_t condicionAccionesPyMedico;
pthread_cond_t condicionAccionesPyEstadistico;

void writeLogMessage(char *id, char *msg);
void mainHandler(int signal);
void nuevoPaciente(int signal);
void *accionesPaciente(void *arg);
void *HiloEnfermero(void *arg);
void accionesEnfermero(char tipo, int id);
void *accionesEstadistico(void *arg);
void *accionesMedico(void *arg);
void accionesMedico2(struct Paciente auxPaciente);
void *HiloPaciente(void *arg);

int main(int argc, char** argv) {
    //TODO Inicializar semaforos/mutex/var condicion no implementadas todavía
    for(int i=0; i<MAXPACIENTES; i++) {
        pacientes[i].id = 0;
        pacientes[i].atendido = 0;
        pacientes[i].tipo = '0';
        pacientes[i].serologia = false;
    }

    for(int i=0; i<ENFERMEROS; i++) {
        enfermeros[i].id = i+1;
        if(i == 0) enfermeros[i].tipo = 'J';
        if(i == 1) enfermeros[i].tipo = 'M';
        if(i == 2) enfermeros[i].tipo = 'S';
        pthread_create(&enfermero, NULL, HiloEnfermero, &i);
    }

    pthread_mutex_init(&mutex_enf, NULL);

    //inicializacion del mutex
    //para usarlo: pthread_mutex_lock(&mutex_hilos) y lo mismo con unlock
    pthread_mutex_init(&mutex_hilos,NULL);

    /*VARIABLE CONDICION(REVISAR Y CAMBIAR NOMBRE)
    *basicamente es una variable inicializada a 0 que segun quien la use aumenta su valor,
    * lo disminuye, o espera a que tenga un valor
    */

    pthread_create(&medico,NULL,accionesMedico,NULL);
    pthread_create(&estadistico,NULL,accionesEstadistico,NULL);
    //bucle que espera señales infinitamente
    while(1){
        signal(SIGUSR1, mainHandler);   //Junior
        signal(SIGUSR2, mainHandler);   //Medio
        signal(SIGPIPE, mainHandler);   //Senior
        signal(SIGINT, mainHandler);    //Terminar programa
    }


}

void writeLogMessage(char *id, char *msg) {
// Calculamos la hora actual
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);
// Escribimos en el log
    logFile = fopen("logFileName", "a");
    fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
    fclose(logFile);
}

void mainHandler(int signal) {
    switch(signal) {
        case SIGUSR1:
            //Generar paciente Junior
            nuevoPaciente(SIGUSR1);
            break;
        case SIGUSR2:
            //Generar paciente Medio
            nuevoPaciente(SIGUSR2);
            break;
        case SIGPIPE:
            //Generar paciente Senior
            nuevoPaciente(SIGPIPE);
            break;
        case SIGINT:
            //Salir del programa
            exit(0);
            break;
    }
}

//metodo que comprueba si hay sitio para la llegada de un nuevo paciente, y en ese caso lo crea
void nuevoPaciente(int signal_handler)
{
    struct Paciente p;
    for(int i = 1;i<=MAXPACIENTES;i++){
        if(pacientes[i].id == 0){
            //si hay espacio, creamos un nuevo paciente y lo añadimos al array de pacientes
            pacientes[i].id = i;
            pacientes[i].atendido = 0;
            //13 = SIGPIPE; 16 = SIGUSR1; 17 = SIGUSR2
            //TODO comprobar que funciona el switch, sino poner casos para todos los numeros posibles
            switch(signal_handler){
                case SIGPIPE:
                    //paciente senior
                    pacientes[i].tipo = 'S';
                    break;
                case SIGUSR1:
                    //paciente junior
                    pacientes[i].tipo = 'J';
                    break;
                case SIGUSR2:
                    //paciente medio
                    pacientes[i].tipo = 'M';
                    break;
            }
            pacientes[i].serologia = false;
            p.serologia = false;
            p.atendido = 0;
            p.id = i;
            p.tipo = pacientes[i].tipo;
            pthread_create(&hilo_paciente,NULL,accionesPaciente,(void *)&p);
            break;
        }
    }
}

#pragma ide diagnostic ignored "EndlessLoop"
void *HiloEnfermero(void *arg) {
    while(true) {
        pthread_mutex_lock(&mutex_enf); //Lock enfermero
        int i = *(int*)arg;
        int id = enfermeros[i].id;
        char tipo = enfermeros[i].tipo;
        pthread_mutex_unlock(&mutex_enf);//Unlock enfermero

        char str[] = "Enfermer@ ";
        char idChar = 'id';
        strncat(str, &idChar, 1);
        writeLogMessage(str, "Ateniendo a paciente");
        accionesEnfermero(tipo, id);
    }
}

void accionesEnfermero(char tipo, int id) {
    srand(time(NULL));
    bool otroTipo = true;  //True si no se ha atendido a un apciente de su tipo y va a otro rango de edad
    bool vacio = true; //True si no hay pacientes

    char str[] = "Enfermer@ ";
    char idChar = 'id';
    strncat(str, &idChar, 1);

    if(tipo=='J' || tipo=='M' || tipo=='S') {   // Tipo valido
        //TODO MUTEX DE PACIENTES LOCK
        for(int i = 0; i<MAXPACIENTES; i++) {
            if(pacientes[i].tipo == tipo && !pacientes[i].atendido) {
                otroTipo = false; vacio = false;
                pacientes[i].atendido = true;
                int random = (rand()%100)+1;    //Random entre 0 y 100
                writeLogMessage("Enfermer@", "Comienza la atención del paciente");
                if(random<=80) {    //To_do en regla
                    sleep((rand()%4)+1);
                    if(pacientes[i].serologia){
                        //TODO Se le manda a estudio
                        // TODO Abandona consultorio, revisar si se va aqui o en estudio
                        // pacientes[i].id = 0;
                    }
                    writeLogMessage("Enfermer@", "Fin atención paciente exitosa");
                } else if(random>80 && random<= 90) {  //Mal identificados
                    sleep((rand()%5)+2);
                    if(pacientes[i].serologia){
                        // TODO Se le manda a estudio
                        // TODO Abandona consultorio, revisar si se va aqui o en estudio
                        // pacientes[i].id = 0;
                    }
                    writeLogMessage("Enfermer@", "Fin atención paciente exitosa");
                } else if(random>90 && random<=100){   //Catarro o Gripe
                    sleep((rand()%5)+6);
                    writeLogMessage("Enfermer@", "Fin atención paciente: Catarro o Gripe");
                    pacientes[i].id = 0;    //Borra paciente
                    //Abandonan consultorio sin reaccion ni estudio
                }
            }
            if(contEnfermero == 5) {    //Comprobamos descanso para cafe
                writeLogMessage("Enfermer@", "Descanso para el cafe");
                sleep(5);
            }
        }
        if(vacio) {
            sleep(1);
            accionesEnfermero(tipo, id);
            return;
        } else if(otroTipo) {  //Atendemos a otro paciente de otro rango de edad
            for(int j = 0; j<MAXPACIENTES; j++) {
                if(!pacientes[j].atendido) {
                    accionesEnfermero(pacientes[j].tipo, id);
                    return;
                }
            }
        }
    } else {    //Tipo invalido
        perror("Emfermero sin tipo valido");
        return;
    }
}

void *accionesEstadistico(void *arg)
{

//espera que le avisen de que hay un paciente en estudio (EXCLUSION MUTUA)
    pthread_join(&hilo_paciente,NULL);
//escribe en el log el comienzo de actividad (EXCLUSION MUTUA)
    pthread_mutex_lock(&mutex_hilos);
    writeLogMessage("Estadistico","Comienzo de actividad del estadistico.");
//calcula el tiempo de actividad
    sleep(4);
//termina la actividad y avisa al paciente (VARIABLES CONDICION)
    pthread_cond_signal(&condicion);
//escribe en el log que finaliza la actividad (EXCLUSION MUTUA)
    writeLogMessage("Estadistico","Fin de actividad del estadistico.");
//cambia paciente en estudio y vuelve a 1 (EXCLUSION MUTUA)
    pthread_mutex_unlock(&mutex_hilos);
}

void *accionesPaciente(void *arg){
    struct Paciente *p;
    pthread_mutex_lock(&mutexAccionesPaciente);
    p=(struct Paciente *)arg;
    //Guardar en el log la hora de entrada.
    writeLogMessage("Paciente","Hora de entrada del paciente.");
    //Guardar en el log el tipo de solicitud.
    char mensajeTipoPaciente[50];
    char tipoPaciente[]="Tipo de solicitud del paciente : ";
    strcpy(mensajeTipoPaciente,tipoPaciente);
    strcat(mensajeTipoPaciente,p->tipo);
    writeLogMessage("Paciente",mensajeTipoPaciente);
    //Duerme 3 segundos
    sleep(3);
    //Comprueba si está siendo atendido.
    //Si no lo está, calculamos el comportamiento del paciente (si se va por cansarse
    //de esperar, si se lo piensa mejor) o si se va al baño y pierde su turno.
    if(p->atendido==0){
        int comportamientoPaciente=rand()% 100+1;
        //un 20 % de los pacientes se cansa de esperar y se va.
        if(comportamientoPaciente<=20){
            //Log que avisa de que se va por cansancio
            writeLogMessage("Paciente","El paciente se ha ido porque se ha cansado de esperar.");
            //codigo de cuando se va
            p->id=0;
            pthread_exit(NULL);
        }else if(comportamientoPaciente>20&&comportamientoPaciente<=30){
            //Log que avisa de que se va porque se lo ha pensado mejor
            writeLogMessage("Paciente","El paciente se lo ha pensado mejor y se ha ido.");
            //codigo de cuando se lo piensa mejor y se va tambien.
            p->id=0;
            pthread_exit(NULL);
        }else{
            //70% restante
            int comportamientoPacRestantes=rand()% 100+1;
            if(comportamientoPacRestantes<=5){
                //Log que avisa de que ha perdido el turno por ir al baño
                writeLogMessage("Paciente","El paciente ha ido al baño y ha perdido el turno.");
                //Codigo de cuando se va al baño y pierde el turno.
                p->id=0;
                pthread_exit(NULL);
            }else{
                //Codigo de los pacientes que ni se van ni pierden turno.
                //El paciente debe dormir 3 segundos y vuelve a 4.
                sleep(3);
                p->atendido=4;
            }
        }
    }else{
        //Si está siendo atendido por el enfermer@ debemos esperar a que termine.
        pthread_cond_wait(&condicionAccionesPyEnfermero,&mutexAccionesPaciente);
    }
    //Si no se va por gripe o catarro calcula si le da reacción
    //TIENE QUE ESPERAR INFORMACION DEL MEDICO Y/O ENFERMEROS
    pthread_cond_wait(&condicionInfoMedicoyEnfermero,&mutexAccionesPaciente);
    int reaccionPaciente=rand()% 100+1;
    if(reaccionPaciente<=10){
        //Si le da cambia el valor de atendido a 4
        p->atendido=4;
        //Esperamos a que termine la atención
        pthread_cond_wait(&condicionAccionesPyMedico,&mutexAccionesPaciente);
    }else{
        //Si no le da reacción calculamos si decide o no participar en el estudio serológico
        int participaEstudio=rand()% 100+1;
        if(participaEstudio<=25){
            //Si decide participar
            //Cambia el valor de la variable serológica
            p->serologia=true;
            //Cambia el valor de paciente en estudio.
            //Avisa al estadistico
            pthread_cond_signal(&condicionAccionesPyEstadistico);
            //Guardamos el log en que está preparado para el estudio
            writeLogMessage("Paciente","El paciente está preparado para el estudio.");
            //Se queda esperando a que digan que pueden marchar
            pthread_cond_wait(&condicionAccionesPyEstadistico,&mutexAccionesPaciente);
            //Guardamos el log en que deja el estudio
            writeLogMessage("Paciente","El paciente ha terminado el estudio.");
        }
    }
    //Libera su posición en cola de solicitudes y se va
    p->id=0;
    //Escribe en el log
    writeLogMessage("Paciente","El paciente ha terminado de vacunarse y se ha ido.");
    pthread_exit(NULL);
    pthread_mutex_unlock(&mutexAccionesPaciente);
    //Fin del hilo Paciente.
}

void *accionesMedico(void *arg){
    int junior = 0,  medio = 0,  senior = 0; //num pacientes Junior, Medio y Senior
    //buscamos al paciente CON REACCIÓN que más tiempo lleve esperando
    for(int i = 0; i < numPacientes; i++){
        pthread_mutex_lock(&mutex_hilos);
        //si hay con reaccion
        //esperar señal de accionesPaciente
        //una vez recibida enviarle otra de nuevo
        if(pacientes[i].atendido == 4){
            sleep(5);
            /*pacientes[i].id = 0;
            pacientes[i].serologia = 0;
            pacientes[i].tipo = 0;
            pacientes[i].atendido = 0;*/
            //pthread_cond_wait(&condicion, &mutex_hilos);
            //pthread_cond_signal(&condicion);
        }
            //si no, escogemos al que mas lleve esperando
        else{//calculamos la cola con mas solicitudes
            for(int j = 0; j < numPacientes; j++){
                if(pacientes[j].tipo == 'J') {
                    junior++;
                }
                else if(pacientes[j].tipo == 'M') {
                    medio++;
                }
                else {
                    senior++;
                }
            }
            //COMPROBAR QUE EL ID NO SEA 0
            for(int k = 0; k < numPacientes; k++){//atendemos a aquel de la cola con mas solicitudes y que mas tiempo lleve esperando
                if(pacientes[k].id != 0 && pacientes[k].tipo == 'J' && junior >= medio && junior >=senior) {
                    accionesMedico2(pacientes[k]);
                    break;
                }
                else if(pacientes[k].id != 0 && pacientes[k].tipo == 'M' && medio >= junior && medio >=senior){
                    accionesMedico2(pacientes[k]);
                    break;
                }
                else if(pacientes[k].id != 0 && pacientes[k].tipo == 'S' && senior >= junior && senior >= medio){
                    accionesMedico2(pacientes[k]);
                    break;
                }
            }
            //sino hay pacientes esperando
            if(junior == 0 && medio == 0 && senior == 0){
                //esperamos 1 segundo
                sleep(1);
                //volvemos al primero
                i = 0;
            }
        }
        pthread_mutex_unlock(&mutex_hilos);
    }
}

void accionesMedico2(struct Paciente auxPaciente){
    int tipoAtencion = 0; //si es reaccion o vacunacion
    int tiempoEspera = 0; //tiempo que espera el paciente
    int tieneReaccion = rand()% 100+1; //si este valor es <=10 tiene reaccion
    int vaAlEstudio= rand()% 100+1; //si este valor es <= 25 va al estudio serologico
    auxPaciente.atendido = 1;

    tipoAtencion = rand()% 100+1;//calculamos el tipo de atencion

    //Guardar en el log la hora de entrada.
    writeLogMessage("Paciente","Hora de entrada del paciente.");

    //paciente está en regla
    if(tipoAtencion <= 80){
        tiempoEspera =  rand()% 4+1; //el tiempo de espera está entre 1 y 4 segundos
        sleep(tiempoEspera);

        if(tieneReaccion <= 10) {//comprueba si hay reaccion
            auxPaciente.atendido = 4;
        }
        if(vaAlEstudio <= 25) {//comprueba si participa en el estudio
            auxPaciente.serologia = true;
            //pasa señal al estadistico
            pthread_cond_signal(&condicion);
        }

        //motivo finalización atención
        writeLogMessage("Paciente","El paciente fue atendido con éxito.");
    }
        //paciente está mal identificado
    else if(tipoAtencion > 80 && tipoAtencion <= 90){
        tiempoEspera =  rand()% 5+2; //el tiempo de espera está entre 2 y 6 segundos
        sleep(tiempoEspera);

        if(tieneReaccion <= 10) {//comprueba si hay reaccion
            auxPaciente.atendido = 4;
        }
        if(vaAlEstudio <= 25) {//comprueba si participa en el estudio
            auxPaciente.serologia = true;
            //pasa señal al estadistico
            pthread_cond_signal(&condicion);
        }

        //motivo finalización atencion
        writeLogMessage("Paciente","El paciente estaba mal identificado.");
    }
        //paciente tiene catarro o gripe
    else{
        tiempoEspera =  rand()% 5+6; //el tiempo de espera está entre 6 y 10 segundos
        sleep(tiempoEspera);
        //no se vacunan
        //no participan en el estudio
        //abandona consultorio
        auxPaciente.id = 0;
        //motivo finalización atención
        writeLogMessage("Paciente","El paciente tenía catarro o gripe.");
    }

    //finaliza la atención
    writeLogMessage("Paciente","El paciente fue atendido.");

}
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
    int atendido;   // 0 no atendido, 1 atendido, 2 catarro/gripe
    char tipo;  // Junior J, Medio M, Senior S
    bool serologia;
};

struct Enfermero {
    int id;
    char tipo;  // Junior J, Medio M, Senior S
    int contEnfermero;  //Contador de pacientes atendidos para café
};

struct Paciente pacientes[MAXPACIENTES];
struct Enfermero enfermeros[ENFERMEROS];
int numPacientes;
bool fin;
pthread_t medico, estadistico, enfermero[ENFERMEROS], hilo_paciente;
pthread_mutex_t mutex_paciente, mutex_estadistico, mutex_enf;
FILE* logFile;
pthread_cond_t condicion;
pthread_cond_t condicionAccionesPyEnfermero;
pthread_cond_t condicionInfoMedicoyEnfermero;
pthread_cond_t condicionAccionesPyMedico;
pthread_cond_t condicionAccionesPyEstadistico;

void writeLogMessage(char *id, char *msg);
bool isColaVacia();
void mainHandler(int signal);
void nuevoPaciente(int signal);
void *accionesPaciente(void *arg);
void *HiloEnfermero(void *arg);
void accionesEnfermero(char tipo, int id);
void *accionesEstadistico(void *arg);
void *accionesMedico(void *arg);
void accionesMedico2(struct Paciente auxPaciente);
void *HiloPaciente(void *arg);

#pragma ide diagnostic ignored "EndlessLoop"
int main(int argc, char** argv) {
    signal(SIGUSR1, mainHandler);   //Junior
    signal(SIGUSR2, mainHandler);   //Medio
    signal(SIGPIPE, mainHandler);   //Senior
    signal(SIGINT, mainHandler);    //Terminar programa

    // Inicialización del mutex - TODO Gestión de errores de mutex/thread
    pthread_mutex_init(&mutex_enf, NULL);
    pthread_mutex_init(&mutex_paciente, NULL);

    for(int i=0; i<MAXPACIENTES; i++) {
        pacientes[i].id = 0;
        pacientes[i].atendido = 0;
        pacientes[i].tipo = '0';
        pacientes[i].serologia = false;
    }

    for(int i=0; i<ENFERMEROS; i++) {
        enfermeros[i].id = i+1;
        enfermeros[i].contEnfermero = 0;
        if(i == 0) enfermeros[i].tipo = 'J';
        if(i == 1) enfermeros[i].tipo = 'M';
        if(i == 2) enfermeros[i].tipo = 'S';
        pthread_create(&enfermero[i], NULL, HiloEnfermero, &i);
        usleep(100);
    }

    pthread_create(&medico,NULL,accionesMedico,NULL);
    pthread_create(&estadistico,NULL,accionesEstadistico,NULL);

    fin = false;

    while(1) {  //Esperamos por señales
        wait(0);
        sleep(1);
        if(fin) {   //Recibido SIGINT, acaba el programa
            break;
        }
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
            nuevoPaciente(SIGUSR1); //Generar paciente Junior
            break;
        case SIGUSR2:
            nuevoPaciente(SIGUSR2); //Generar paciente Medio
            break;
        case SIGPIPE:
            nuevoPaciente(SIGPIPE); //Generar paciente Senior
            break;
        case SIGINT:
            //TODO Salir del programa
            exit(0);
            break;
        default:
            break;
    }
}

//metodo que comprueba si hay sitio para la llegada de un nuevo paciente, y en ese caso lo crea
void nuevoPaciente(int signal_handler)
{
    pthread_mutex_lock(&mutex_paciente);
    for(int i = 0;i<MAXPACIENTES;i++){
        if(pacientes[i].id == 0){
            //si hay espacio, creamos un nuevo paciente y lo añadimos al array de pacientes
            pacientes[i].id = i+1;
            pacientes[i].atendido = 0;
            //13 = SIGPIPE; 16 = SIGUSR1; 17 = SIGUSR2
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
                default:
                    break;
            }
            pacientes[i].serologia = false;
            writeLogMessage("Paciente", "Creando paciente...");
            pthread_create(&hilo_paciente,NULL,accionesPaciente,&i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_paciente);
}

void *HiloEnfermero(void *arg) {
    int i = *(int*)arg;     //Convertimos argumento
    pthread_mutex_lock(&mutex_enf);     //Lock enfermero
    int id = enfermeros[i].id;
    char tipo = enfermeros[i].tipo;
    char str[] = "Enfermer@ ";
    char idChar = id+'0';
    strncat(str, &idChar, 1);
    writeLogMessage(str, "Iniciado, comenzando atención");
    pthread_mutex_unlock(&mutex_enf);   //Unlock enfermero
    while(1) {
        accionesEnfermero(tipo, id);
    }
}

bool isColaVacia() {
    bool vacio = true;
    pthread_mutex_lock(&mutex_paciente);
    for(int i = 0; i<MAXPACIENTES; i++) {
        if(pacientes[i].id != 0) {
            vacio = false;
        }
    }
    pthread_mutex_unlock(&mutex_paciente);
    return vacio;
}

#pragma ide diagnostic ignored "EndlessLoop"
void accionesEnfermero(char tipo, int id) {     //TODO Terminar código
    srand(time(NULL));
    bool otroTipo = true;  //True si no se ha atendido a un apciente de su tipo y va a otro rango de edad

    char str[] = "Enfermer@ ";
    char idChar = id+'0';
    strncat(str, &idChar, 1);

    while(isColaVacia()) {
        sleep(2);
    }
    if(tipo=='J' || tipo=='M' || tipo=='S') {   // Tipo valido
        pthread_mutex_lock(&mutex_paciente); //Lock paciente
        for(int i = 0; i<MAXPACIENTES; i++) {
            if(pacientes[i].tipo == tipo && pacientes[i].atendido == 0) {
                otroTipo = false;
                pacientes[i].atendido = 1;
                int random = (rand()%100)+1;    //Random entre 0 y 100
                writeLogMessage(str, "Comienza la atención del paciente");
                if(random<=80) {    //To_do en regla
                    writeLogMessage(str, "Paciente con todo en regla, atendiendo...");
                    sleep((rand()%4)+1);
                    pthread_mutex_lock(&mutex_enf);
                    enfermeros[i].contEnfermero++;  //Aumentamos contador de pacientes
                    pthread_mutex_unlock(&mutex_enf);
                    pthread_mutex_unlock(&mutex_paciente);
                    writeLogMessage(str, "Fin atención paciente exitosa");
                    //Enviamos señal al accionesPaciente cuando termina la atención
                    pthread_cond_signal(&condicionInfoMedicoyEnfermero);
                    break;
                } else if(random>80 && random<= 90) {  //Mal identificados
                    writeLogMessage(str, "Paciente mal identificado, atendiendo...");
                    sleep((rand()%5)+2);
                    pthread_mutex_lock(&mutex_enf);
                    enfermeros[i].contEnfermero++;  //Aumentamos contador de pacientes
                    pthread_mutex_unlock(&mutex_enf);
                    pthread_mutex_unlock(&mutex_paciente);
                    writeLogMessage(str, "Fin atención paciente exitosa");
                    //Enviamos señal al accionesPaciente cuando termina la atención
                    pthread_cond_signal(&condicionInfoMedicoyEnfermero);
                    break;
                } else if(random>90 && random<=100){   //Catarro o Gripe
                    writeLogMessage(str, "Paciente con catarro o gripe");
                    sleep((rand()%5)+6);
                    pacientes[i].atendido = 2;
                    writeLogMessage(str, "Fin atención paciente: Catarro o Gripe");
                    pthread_mutex_unlock(&mutex_paciente);
                    //Enviamos señal al accionesPaciente cuando termina la atención
                    pthread_cond_signal(&condicionInfoMedicoyEnfermero);
                }
            }
            pthread_mutex_lock(&mutex_enf);
            if(enfermeros[i].contEnfermero == 5) {    //Comprobamos descanso para cafe
                writeLogMessage(str, "Descanso para el cafe");
                sleep(5);
                enfermeros[i].contEnfermero = 0;
            }
            pthread_mutex_unlock(&mutex_enf);
        }
        /*if(otroTipo) {  //Atendemos a otro paciente de otro rango de edad
            for(int j = 0; j<MAXPACIENTES; j++) {
                if(!pacientes[j].atendido) {
                    accionesEnfermero(pacientes[j].tipo, id);
                    return;
                }
        }*/
    } else {    //Tipo invalido
        writeLogMessage(str, "ERROR: Tipo inválido, cesando actividad de enfermero");
        perror("Enfermero sin tipo valido");
        return;
    }


}

void *accionesEstadistico(void *arg)    //TODO Terrorista
{

//espera que le avisen de que hay un paciente en estudio (EXCLUSION MUTUA)
    pthread_cond_wait(&condicionAccionesPyEstadistico, &mutex_estadistico);
//escribe en el log el comienzo de actividad (EXCLUSION MUTUA)
    pthread_mutex_lock(&mutex_estadistico);
    writeLogMessage("Estadistico","Comienzo de actividad del estadistico.");
//calcula el tiempo de actividad
    sleep(4);
//termina la actividad y avisa al paciente (VARIABLES CONDICION)
    pthread_cond_signal(&condicionAccionesPyEstadistico);
//escribe en el log que finaliza la actividad (EXCLUSION MUTUA)
    writeLogMessage("Estadistico","Fin de actividad del estadistico.");
//cambia paciente en estudio y vuelve a 1 (EXCLUSION MUTUA)
    pthread_mutex_unlock(&mutex_estadistico);
}

void *accionesPaciente(void *arg){ //TODO Señal médico atendido (hecho)
    // TODO Mirar señales estadístico (hecho)

    int i=*(int*)arg;
    pthread_mutex_lock(&mutex_paciente);
    //Guardar en el log la hora de entrada.
    writeLogMessage("Paciente","Hora de entrada del paciente.");
    //Guardar en el log el tipo de solicitud.
    char tipoPaciente[]="Tipo del paciente: ";
    char tipoP = pacientes[i].tipo;
    strncat(tipoPaciente, &tipoP, 1);
    writeLogMessage("Paciente",tipoPaciente);
    //Duerme 3 segundos
    sleep(3);
    //Comprueba si está siendo atendido.
    //Si no lo está, calculamos el comportamiento del paciente (si se va por cansarse
    //de esperar, si se lo piensa mejor) o si se va al baño y pierde su turno.
    pthread_mutex_unlock(&mutex_paciente);
    while(pacientes[i].atendido==0){
        int comportamientoPaciente=rand()% 100+1;
        //un 20 % de los pacientes se cansa de esperar y se va.
        if(comportamientoPaciente<=20){
            pthread_mutex_lock(&mutex_paciente);
            //Log que avisa de que se va por cansancio
            writeLogMessage("Paciente","El paciente se ha ido porque se ha cansado de esperar.");
            //codigo de cuando se va
            pacientes[i].id=0;
            pthread_mutex_unlock(&mutex_paciente);
            pthread_exit(NULL);
        }else if(comportamientoPaciente>20&&comportamientoPaciente<=30){
            pthread_mutex_lock(&mutex_paciente);
            //Log que avisa de que se va porque se lo ha pensado mejor
            writeLogMessage("Paciente","El paciente se lo ha pensado mejor y se ha ido.");
            //codigo de cuando se lo piensa mejor y se va tambien.
            pacientes[i].id=0;
            pthread_mutex_unlock(&mutex_paciente);
            pthread_exit(NULL);
        }else{
            //70% restante
            int comportamientoPacRestantes=rand()% 100+1;
            if(comportamientoPacRestantes<=5){
                pthread_mutex_lock(&mutex_paciente);
                //Log que avisa de que ha perdido el turno por ir al baño
                writeLogMessage("Paciente","El paciente ha ido al baño y ha perdido el turno.");
                //Codigo de cuando se va al baño y pierde el turno.
                pacientes[i].id=0;
                pthread_mutex_unlock(&mutex_paciente);
                pthread_exit(NULL);
            }else{
                //Codigo de los pacientes que ni se van ni pierden turno.
                //El paciente debe dormir 3 segundos y vuelve a 4.
                sleep(3);
            }
        }
    }
    //Si está siendo atendido por el enfermer@ debemos esperar a que termine.
    //pthread_cond_wait(&condicionAccionesPyEnfermero,&mutex_paciente);
    //Si no se va por gripe o catarro calcula si le da reacción
    //TIENE QUE ESPERAR INFORMACION DEL MEDICO Y/O ENFERMEROS
    pthread_mutex_lock(&mutex_paciente);
    pthread_cond_wait(&condicionInfoMedicoyEnfermero,&mutex_paciente);
    if(pacientes[i].atendido==2) {
        pacientes[i].id = 0;
        pthread_mutex_unlock(&mutex_paciente);
        pthread_exit(NULL);
    }
    int reaccionPaciente=rand()% 100+1;
    if(reaccionPaciente<=10){
        //Si le da cambia el valor de atendido a 4
        pacientes[i].atendido=4;
        //Esperamos a que termine la atención
        pthread_cond_wait(&condicionAccionesPyMedico,&mutex_paciente);
    }else{
        //Si no le da reacción calculamos si decide o no participar en el estudio serológico
        int participaEstudio=rand()% 100+1;
        if(participaEstudio<=25){
            pthread_mutex_lock(&mutex_paciente);
            //Si decide participar
            //Cambia el valor de la variable serológica
            pacientes[i].serologia=true;
            //Cambia el valor de paciente en estudio.
            //Avisa al estadistico
            pthread_cond_signal(&condicionAccionesPyEstadistico);
            //Guardamos el log en que está preparado para el estudio
            writeLogMessage("Paciente","El paciente está preparado para el estudio.");
            //Se queda esperando a que digan que pueden marchar
            pthread_cond_wait(&condicionAccionesPyEstadistico,&mutex_paciente);
            //Guardamos el log en que deja el estudio
            writeLogMessage("Paciente","El paciente ha terminado el estudio.");
        }
    }
    //Libera su posición en cola de solicitudes y se va
    pacientes[i].id=0;
    //Escribe en el log
    writeLogMessage("Paciente","El paciente ha terminado de vacunarse y se ha ido.");
    pthread_mutex_unlock(&mutex_paciente);
    pthread_exit(NULL);
    //Fin del hilo Paciente.
}

void *accionesMedico(void *arg){
    int junior = 0,  medio = 0,  senior = 0; //num pacientes Junior, Medio y Senior
    //buscamos al paciente CON REACCIÓN que más tiempo lleve esperando
    for(int i = 0; i < numPacientes; i++){
        pthread_mutex_lock(&mutex_paciente);
        //si hay con reaccion
        //esperar señal de accionesPaciente
        if(pacientes[i].atendido == 4){
            sleep(5);
            pthread_cond_signal(&condicionInfoMedicoyEnfermero);
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
        pthread_mutex_unlock(&mutex_paciente);
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
    writeLogMessage("Médico","Hora de entrada del paciente.");

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
            pthread_cond_signal(&condicionAccionesPyEstadistico);
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
            pthread_cond_signal(&condicionAccionesPyEstadistico);
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
        pthread_cond_signal(&condicionInfoMedicoyEnfermero);

        //motivo finalización atención
        writeLogMessage("Paciente","El paciente tenía catarro o gripe.");
    }

    //finaliza la atención
    writeLogMessage("Paciente","El paciente fue atendido.");

}
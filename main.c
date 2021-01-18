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
#include <limits.h>

#define MAXPACIENTES 15
#define ENFERMEROS 3

struct Paciente {
    int id;
    int atendido;   // -1 no atendido, 0 iniciado, 1 atendido, 2 catarro/gripe
    int prioridad;  // Más bajo = mayor prioridad
    char tipo;  // Junior J, Medio M, Senior S
    bool serologia;
    pthread_t hilo_paciente;
};

struct Enfermero {
    int id;
    char tipo;  // Junior J, Medio M, Senior S
    int contEnfermero;  //Contador de pacientes atendidos para café
};

struct Paciente pacientes[MAXPACIENTES];
struct Enfermero enfermeros[ENFERMEROS];
int numPacientes, contPrioridad;
pthread_t medico, estadistico, enfermero[ENFERMEROS];
pthread_mutex_t mutex_paciente, mutex_estadistico;
pthread_cond_t cond_pac[MAXPACIENTES], cond_est, cond_med;
FILE* logFile;
bool ignore_signals;

bool isColaVacia();
bool isColaVaciaMed();
void mainHandler(int signal);
void nuevoPaciente(int signal);
void *HiloPaciente(void *arg);
void *HiloEnfermero(void *arg);
void accionesEnfermero(char tipo, int id);
void *HiloEstadistico(void *arg);
void *HiloMedico(void *arg);
void accionesMedico(int i);
void eliminaPaciente(int i);
int buscaPaciente(char tipo);
int buscaPacienteMedReaccion();
int buscaPacienteMed();
void writeLogMessage(char *id, char *msg);

#pragma ide diagnostic ignored "EndlessLoop"
int main(int argc, char** argv) {
    signal(SIGUSR1, mainHandler);   //Junior
    signal(SIGUSR2, mainHandler);   //Medio
    signal(SIGPIPE, mainHandler);   //Senior
    signal(SIGINT, mainHandler);    //Terminar programa

    // Inicialización del mutex y variables condición
    pthread_mutex_init(&mutex_paciente, NULL);
    pthread_mutex_init(&mutex_estadistico, NULL);
    pthread_cond_init(&cond_est, NULL);
    pthread_cond_init(&cond_med, NULL);

    for(int i=0; i<MAXPACIENTES; i++) {
        pthread_cond_init(&cond_pac[i], NULL);
        pacientes[i].id = 0;
        pacientes[i].atendido = -1;
        pacientes[i].tipo = '0';
        pacientes[i].serologia = false;
    }

    contPrioridad = 1;
    numPacientes = 1;

    for(int i=0; i<ENFERMEROS; i++) {
        enfermeros[i].id = i+1;
        enfermeros[i].contEnfermero = 0;
        if(i == 0) enfermeros[i].tipo = 'J';
        if(i == 1) enfermeros[i].tipo = 'M';
        if(i == 2) enfermeros[i].tipo = 'S';
        pthread_create(&enfermero[i], NULL, HiloEnfermero, &i);
        usleep(1000);
    }

    pthread_create(&medico,NULL,HiloMedico,NULL);
    pthread_create(&estadistico, NULL, HiloEstadistico, NULL);

    ignore_signals = false;

    while(1) {  //Esperamos por señales
        wait(0);
        sleep(1);
        if(ignore_signals) {   //Recibido SIGINT, acaba el programa
            break;
        }
    }
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
            ignore_signals = true;
            while(isColaVacia() == false){
                usleep(100);
            }
            writeLogMessage("SIGINT","El consultorio se ha cerrado. Fin del programa.");
            exit(0);
        default:
            break;
    }
}

//metodo que comprueba si hay sitio para la llegada de un nuevo paciente, y en ese caso lo crea
void nuevoPaciente(int signal_handler) {
    bool add = false;
    pthread_mutex_lock(&mutex_paciente);
    for(int i = 0;i<MAXPACIENTES;i++){
        if(pacientes[i].id == 0){
            add = true;
            //si hay espacio, creamos un nuevo paciente y lo añadimos al array de pacientes
            pacientes[i].id = numPacientes;
            pacientes[i].prioridad = contPrioridad;
            contPrioridad++;
            numPacientes++;
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
            char str[50];
            sprintf(str, "Generado nuevo paciente con id %d y tipo %c", pacientes[i].id, pacientes[i].tipo);
            writeLogMessage("Consultorio", str);
            pthread_create(&pacientes[i].hilo_paciente, NULL, HiloPaciente, &i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_paciente);
    if(!add) {
        writeLogMessage("Consultorio", "La cola está llena, no se ha añadido el paciente");
    }

}

void *HiloEnfermero(void *arg) {
    int i = *(int*)arg;     //Convertimos argumento
    int id = enfermeros[i].id;
    char tipo = enfermeros[i].tipo;
    char str[15];
    sprintf(str, "Enfermer@ %d", id);
    writeLogMessage(str, "Iniciado, comenzando atención");
    while(1) {
        accionesEnfermero(tipo, id);
    }
}

#pragma ide diagnostic ignored "EndlessLoop"
void accionesEnfermero(char tipo, int id) {
    srand(time(NULL));
    bool otroTipo = true;  //True si no se ha atendido a un paciente de su tipo y va a otro rango de edad
    int j = -1; //Posición del paciente que se va a tratar

    char str[15], mensaje1[50], mensaje2[50], mensaje3[50], mensaje4[50], mensaje5[50];
    sprintf(str, "Enfermer@ %d", id);

    while(isColaVacia()) {
        sleep(1);
    }

    sleep(2);
    if(tipo=='J' || tipo=='M' || tipo=='S') {   // Tipo valido
        pthread_mutex_lock(&mutex_paciente); //Lock paciente
        if((j = buscaPaciente(tipo)) != -1) {
            sprintf(mensaje5, "Paciente %d seleccionado", pacientes[j].id);
            writeLogMessage(str, mensaje5);
            pacientes[j].atendido = 0;
        }

        if(j != -1) {   //Si se ha seleccionado un paciente
            if(pacientes[j].tipo == tipo && pacientes[j].atendido == 0) {
                sprintf(mensaje1, "Paciente %d con todo en regla, atendiendo...", pacientes[j].id);
                sprintf(mensaje2, "Paciente %d mal identificado, atendiendo...", pacientes[j].id);
                sprintf(mensaje3, "Paciente %d con catarro o gripe", pacientes[j].id);
                sprintf(mensaje4, "Comienza la atención del paciente %d", pacientes[j].id);
                otroTipo = false;
                int random = (rand()%100)+1;    //Random entre 0 y 100
                writeLogMessage(str, mensaje4);

                if(random<=80) {    //To_do en regla
                    writeLogMessage(str, mensaje1);
                    pacientes[j].atendido = 1;
                    pthread_mutex_unlock(&mutex_paciente);  //Unlock paciente

                    sleep((rand()%4)+1);

                    enfermeros[j].contEnfermero++;  //Aumentamos contador de pacientes
                    writeLogMessage(str, "Fin atención paciente exitosa");
                } else if(random>80 && random<= 90) {  //Mal identificados
                    writeLogMessage(str, mensaje2);
                    pacientes[j].atendido = 1;
                    pthread_mutex_unlock(&mutex_paciente);  //Unlock paciente

                    sleep((rand()%5)+2);

                    enfermeros[j].contEnfermero++;  //Aumentamos contador de pacientes
                    writeLogMessage(str, "Fin atención paciente exitosa");
                } else if(random>90 && random<=100){   //Catarro o Gripe
                    writeLogMessage(str, mensaje3);
                    pacientes[j].atendido = 2;
                    pthread_mutex_unlock(&mutex_paciente);  //Unlock paciente

                    sleep((rand()%5)+6);

                    enfermeros[j].contEnfermero++;  //Aumentamos contador de pacientes
                    writeLogMessage(str, "Fin atención paciente: Catarro o Gripe");
                }
            }

            sleep(3);

            char mensaje6[50];
            sprintf(mensaje6, "Enviando señal de fin de atención al paciente %d", pacientes[j].id);
            writeLogMessage(str, mensaje6);

            pthread_mutex_lock(&mutex_paciente);
            pthread_cond_signal(&cond_pac[j]);  //Señal al paciente seleccionado
            pthread_mutex_unlock(&mutex_paciente);

            if(enfermeros[j].contEnfermero == 5) {    //Comprobamos descanso para cafe
                writeLogMessage(str, "Descanso para el cafe");
                sleep(5);
                enfermeros[j].contEnfermero = 0;
            }
        }
        //TODO Gestionar de otro tipo
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

void *HiloEstadistico(void *arg) {
    while(1) {
        int j = -1, id = 0;
        //Comprobamos lista de pacientes para ver quién se va a hacer el estudio
        pthread_mutex_lock(&mutex_paciente);
        for(int i = 0; i<MAXPACIENTES; i++) {
            if(pacientes[i].id!=0 && pacientes[i].serologia) {
                j=i;                    //Seleccionamos la posición del paciente
                id = pacientes[i].id;   //Guardamos su id para el log
                //Ponemos var a false para que no se vuelva a seleccionar el mismo
                pacientes[i].serologia = false;
                break;
            }
        }
        pthread_mutex_unlock(&mutex_paciente);

        if(j!=-1){  //Si se ha seleccionado alguien para el estudio empezamos
            pthread_mutex_lock(&mutex_estadistico);
            char str[40], str1[40];
            sprintf(str, "Realizando estudio al paciente %d", id);
            sprintf(str1, "Fin del estudio del paciente %d", id);

            writeLogMessage("Estadistico",str);
            sleep(4);
            writeLogMessage("Estadistico",str1);

            //Enviamos señal de que ya finalizó el estudio
            pthread_cond_signal(&cond_est);
            pthread_mutex_unlock(&mutex_estadistico);
        } else {
            sleep(1);
        }
    }
}

void *HiloPaciente(void *arg) {
    int i=*(int*)arg;

    pthread_mutex_lock(&mutex_paciente);    //Lock paciente
    char str[15], mensajeTipo[30];
    char tipo = pacientes[i].tipo;
    sprintf(str, "Paciente %d", pacientes[i].id);
    sprintf(mensajeTipo, "Soy del tipo %c", tipo);

    writeLogMessage(str,"Paciente entra al consultorio (ver hora)");
    writeLogMessage(str, mensajeTipo);

    sleep(3);   //Espera inicial del paciente

    //Comprueba atendido, si no lo está comprueba comportamiento
    while(pacientes[i].id!=0 && pacientes[i].atendido<=0){
        int comportamientoPaciente=rand()% 100+1;
        if(comportamientoPaciente<=20){ //Un 20% se cansa de esperar y se va
            writeLogMessage(str,"El paciente se ha ido porque se ha cansado de esperar.");
            //Código de cuando se va
            eliminaPaciente(i);
            break;
        }else if(comportamientoPaciente>20&&comportamientoPaciente<=30){ //Otro 10% se lo piensa mejor y se va
            writeLogMessage(str,"El paciente se lo ha pensado mejor y se ha ido.");
            eliminaPaciente(i);
            break;
        }else{ //70% restante
            int comportamientoPacRestantes=rand()% 100+1;
            if(comportamientoPacRestantes<=5){  //Va al baño y pierde el turno
                writeLogMessage(str,"El paciente ha ido al baño y ha perdido el turno.");
                eliminaPaciente(i);
                break;
            }else{
                pthread_mutex_unlock(&mutex_paciente);
                sleep(3);   //Ni se va ni pierde turno, espera 3 segundos
                pthread_mutex_lock(&mutex_paciente);
            }
        }
    }

    if(pacientes[i].id!=0){     //Si el paciente no se ha ido recibimos señal de enfermero/medico
        writeLogMessage(str, "Fin de comprobaciones de paciente");
        writeLogMessage(str, "Espero señal del enfermero o del médico");
        pthread_cond_wait(&cond_pac[i], &mutex_paciente);
        writeLogMessage(str, "Señal recibida, continuamos");
    } else {                    //Si se ha ido, guardamos el mensaje y salimos
        writeLogMessage(str, "Salgo del consultorio");
        pthread_mutex_unlock(&mutex_paciente);
        pthread_exit(NULL);
    }

    if(pacientes[i].atendido==2) {  //Si tiene catarro o gripe sale del consultorio
        eliminaPaciente(i);
        pthread_mutex_unlock(&mutex_paciente);
        writeLogMessage(str, "Tengo catarro o gripe, salgo del consultorio");
        pthread_exit(NULL);
    }

    int reaccionPaciente=rand()% 100+1;
    if(reaccionPaciente<=10){
        pacientes[i].atendido=4;    //Si le da reacción cambia el valor de atendido a 4
        writeLogMessage(str, "La vacuna me ha dado reacción");
        //Esperamos a que termine la atención
        pthread_cond_wait(&cond_med, &mutex_paciente);
    }else{
        //Si no le da reacción calculamos si decide o no participar en el estudio serológico
        int participaEstudio=rand()% 100+1;
        if(participaEstudio<=25){   //Participa en el estudio
            //Bloqueamos mutex estadistico para evitar que se envíen señales antes del wait
            pthread_mutex_lock(&mutex_estadistico);
            pacientes[i].serologia=true;
            pthread_mutex_unlock(&mutex_paciente);
            writeLogMessage(str,"El paciente está preparado para el estudio");
            //Esperamos señal de que ya acabó el estudio
            pthread_cond_wait(&cond_est, &mutex_estadistico);
            pthread_mutex_unlock(&mutex_estadistico);
            writeLogMessage(str, "Ya he terminado el estudio");
        }
    }
    //Libera su posición en cola de solicitudes y se va
    eliminaPaciente(i);
    writeLogMessage(str,"El paciente ha terminado de vacunarse y se ha ido");
    pthread_mutex_unlock(&mutex_paciente);
    pthread_exit(NULL);
    //Fin del hilo Paciente.
}

void *HiloMedico(void *arg){
    int junior = 0,  medio = 0,  senior = 0, i = -1; //num pacientes Junior, Medio y Senior
    //Buscamos al paciente CON REACCIÓN que más tiempo lleve esperando
    while(1) {
        while(isColaVaciaMed()) {
            sleep(1);
        }

        pthread_mutex_lock(&mutex_paciente);
        if((i = buscaPacienteMedReaccion()) != -1) {    //Tiene reacción (atendido == 4)
            pacientes[i].atendido = 0;  //Lo reiniciamos para que no lo coja de nuevo en la siguiente
            char str[50];
            sprintf(str, "Atendiendo paciente %d con reacción", pacientes[i].id);
            writeLogMessage("Médico", str);
            sleep(5);
            pthread_cond_signal(&cond_med);
            pthread_mutex_unlock(&mutex_paciente);
        } else {    //No hay pacientes con reacción, atendemos a pacientes por orden de prioridad
            //Comprobamos cuál es la cola de pacientes que más pacientes tiene esperando
            for (int j = 0; j < MAXPACIENTES; j++) {
                if (j == 0) {
                    junior = 0; medio = 0; senior = 0;
                }
                if(pacientes[i].id != 0) {
                    if (pacientes[j].tipo == 'J') {
                        junior++;
                    } else if (pacientes[j].tipo == 'M') {
                        medio++;
                    } else if (pacientes[j].tipo == 'S') {
                        senior++;
                    }
                }
            }
            //Atendemos a aquel de la cola con mas solicitudes y que mas tiempo lleve esperando
            if (junior >= medio && junior >= senior) {
                int a = buscaPaciente('J');
                accionesMedico(a);
                break;
            } else if (medio >= junior && medio >= senior) {
                int a = buscaPaciente('M');
                accionesMedico(a);
                break;
            } else if (senior >= junior && senior >= medio) {
                int a = buscaPaciente('S');
                accionesMedico(a);
                break;
            }
        }
    }
}

void accionesMedico(int i) {       //TODO Revisar funcionamiento
    int tipoAtencion = 0; //si es reaccion o vacunacion
    int tiempoEspera = 0; //tiempo que espera el paciente
    pacientes[i].atendido = 1;
    char mensaje1[50], mensaje2[50], mensaje3[50], mensaje4[50];

    sprintf(mensaje1, "Paciente %d con todo en regla, atendiendo...", pacientes[i].id);
    sprintf(mensaje2, "Paciente %d mal identificado, atendiendo...", pacientes[i].id);
    sprintf(mensaje3, "Paciente %d con catarro o gripe", pacientes[i].id);
    sprintf(mensaje4, "Comienza la atención del paciente %d", pacientes[i].id);

    tipoAtencion = rand()% 100+1;   //Calculamos el tipo de atencion

    writeLogMessage("Médico", mensaje4);    //Comienza atención del paciente

    if(tipoAtencion <= 80){                             //Paciente con to_do en regla
        writeLogMessage("Médico",mensaje1);
        tiempoEspera = rand()%4+1;  //Tiempo de espera está entre 1 y 4 segundos
        pacientes[i].atendido = 1;
        pthread_mutex_unlock(&mutex_paciente);  //Unlock mutex
        sleep(tiempoEspera);

    }else if(tipoAtencion > 80 && tipoAtencion <= 90){   //Paciente mal identificado
        writeLogMessage("Médico",mensaje2);
        tiempoEspera =  rand()% 5+2; //Tiempo de espera está entre 2 y 6 segundos
        pacientes[i].atendido = 1;
        pthread_mutex_unlock(&mutex_paciente);  //Unlock mutex
        sleep(tiempoEspera);
    } else if(tipoAtencion >= 90){                       //Paciente tiene catarro o gripe
        writeLogMessage("Médico",mensaje3);
        tiempoEspera =  rand()% 5+6; //Tiempo de espera está entre 6 y 10 segundos
        pacientes[i].atendido = 2;
        pthread_mutex_unlock(&mutex_paciente);  //Unlock mutex
        sleep(tiempoEspera);
    }

    sleep(3);
    writeLogMessage("Médico","Enviando señal de fin de atención al paciente");
    pthread_mutex_lock(&mutex_paciente);    //Lock mutex
    pthread_cond_signal(&cond_pac[i]);  //Señal al paciente seleccionado
    pthread_mutex_unlock(&mutex_paciente);  //Unlock mutex

    //Finaliza la atención
    writeLogMessage("Médico","El paciente fue atendido");
}

bool isColaVacia() {
    bool vacio = true;
    for(int i = 0; i<MAXPACIENTES; i++) {
        if(pacientes[i].id != 0) {
            vacio = false;
        }
    }
    pthread_mutex_unlock(&mutex_paciente);
    return vacio;
}

bool isColaVaciaMed() {
    bool vacio = true;
    for(int i = 0; i<MAXPACIENTES; i++) {
        if(pacientes[i].id != 0) {
            vacio = false;
        }
    }
    return vacio;
}

/*
 * Devuelve el paciente con mayor prioridad de la lista
 * El paciente no debe estar iniciado por nadie (atendido debe ser -1)
 * Si atendido es 0, es que un thread ya lo ha cogido y lo está empezando a atender
 * Precondiciones:  La lista no está vacía, si lo está devolverá -1
 *                  Se inicia la función con el MUTEX YA BLOQUEADO
 *                  El tipo pasado es válido
 */
int buscaPaciente(char tipo) {
    int prioridad = INT_MAX;
    int posicion = -1;
    for(int i = 0; i<MAXPACIENTES; i++) {
        //Si la posicion no está vacía, la prioridad es mayor a la ya guardada y si no se ha atendido
        if(pacientes[i].id != 0 && pacientes[i].prioridad < prioridad &&
           pacientes[i].atendido==-1 && pacientes[i].tipo == tipo) {
            prioridad = pacientes[i].prioridad;
            posicion = i;
        }
    }
    return posicion;
}

/*
 * Devuelve el paciente con mayor prioridad de la lista que le haya dado reacción la vacuna
 * Precondiciones:  La lista no está vacía, si lo está devolverá -1
 *                  Se inicia la función con el MUTEX YA BLOQUEADO
 */
int buscaPacienteMedReaccion() {
    int prioridad = INT_MAX;
    int posicion = -1;
    for(int i = 0; i<MAXPACIENTES; i++) {
        //Si la posicion no está vacía, la prioridad es mayor a la ya guardada y si no se ha atendido
        if(pacientes[i].id != 0 && pacientes[i].prioridad < prioridad &&
           pacientes[i].atendido==4) {
            prioridad = pacientes[i].prioridad;
            posicion = i;
        }
    }
    return posicion;
}

/*
 * Devuelve el paciente con mayor prioridad de la lista que esté sin atender
 * El paciente no debe estar iniciado por nadie (atendido debe ser -1)
 * Si atendido es 0, es que un thread ya lo ha cogido y lo está empezando a atender
 * Precondiciones:  La lista no está vacía, si lo está devolverá -1
 *                  Se inicia la función con el MUTEX YA BLOQUEADO
 */
int buscaPacienteMed() {
    int prioridad = INT_MAX;
    int posicion = -1;
    for(int i = 0; i<MAXPACIENTES; i++) {
        //Si la posicion no está vacía, la prioridad es mayor a la ya guardada y si no se ha atendido
        if(pacientes[i].id != 0 && pacientes[i].prioridad < prioridad &&
           pacientes[i].atendido==-1) {
            prioridad = pacientes[i].prioridad;
            posicion = i;
        }
    }
    return posicion;
}

/*
 * Elimina el paciente indicado por parámetro y pone su valor de atendido a -1
 * Precondiciones: Se inicia la función con el MUTEX YA BLOQUEADO
 */
void eliminaPaciente(int i) {
    pacientes[i].id = 0;
    pacientes[i].atendido = -1;
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
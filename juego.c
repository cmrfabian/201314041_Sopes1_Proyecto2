//REFERENCIAS: 
// - https://stereochro.me/assets/uploads/notes/dcom3/shmem.pdf
// - https://es.wikipedia.org/wiki/Algoritmo_de_Dekker

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/shm.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define PANTALLA_JUEGO_ALTURA 24
#define PANTALLA_JUEGO_ANCHO 70
#define INVASORES 20

#define COMANDANTE_1 "\\   /"
#define COMANDANTE_2 " ||| "

#define DEFENSOR_1 " /|\\"
#define DEFENSOR_2 "/|_|\\"

#define INVASOR_MUERTO "\\x.x/"
#define INVASOR_COMUN "\\-.-/"
#define INVASOR_FUERTE1 "(/-1-\\)"
#define INVASOR_FUERTE2 "(/-2-\\)"
#define INVASOR_FUERTE3 "(/-3-\\)"
#define INVASOR_FUERTE4 "(/-4-\\)"



#if !defined(__GNU_LIBRARY__) || defined(_SEM_SEMUN_UNDEFINED)
union semun
{
    int val; // value for SETVAL
    struct semid_ds* buf; // buffer for IPC_STAT, IPC_SET
    unsigned short* array; // array for GETALL, SETALL
    struct seminfo* __buf; // buffer for IPC_INFO
};
#endif

int CrearSemaforos(int n);
void BorrarSemaforos(int id);
void SemaforoRojo(int id);
void SemaforoVerde(int id);

typedef enum {
    DEF_MOVER_IZQ,
    DEF_MOVER_DER,
    DEF_DISPARAR,
    DEF_NINGUNA,
    DEF_SALIR,
} accion_defensor;

typedef enum {
    INV_MOVER_IZQ,
    INV_MOVER_DER,
    INV_DISPARAR1,
    INV_DISPARAR2,
    INV_DISPARAR3,
    INV_DISPARAR4,
    INV_NINGUNA,
    INV_SALIR,
} accion_invasor;

void game_reset(void);
accion_defensor defensor_obtener_accion(void);
accion_invasor invasor_obtener_accion(void);
void redibujar_pantalla(void);
void defensor_disparar(void);
void invasor_disparar(int nave);
int manipulacion_balas_defensor(void);
int manipulacion_balas_invasor(void);

void manipulacion_invasores(long contador);
void manipulacion_defensor(accion_defensor action);
void manipulacion_comandante(accion_invasor action);

void Dekker_defensor(void);
void Dekker_invasor(void);
void VentanaPrincipal(void);
void VentanaSeleccion(void);
void VentanaEspera(int jug);
void VentanaSalida(int jug);
void Start_SharedMemory(void);
static void exit_game(void);

int *SharedMemory = NULL;

pthread_t tid; 

static void exit_game(void)
{
    endwin();
    printf("Juego Finalizado\n");
}

int main(void)
{
    Start_SharedMemory();
    return 0;
}

void Start_SharedMemory(void )
{
    key_t llave;
    int shm_id;

    //Get llave for shared memory.
    //Key is obtained with any path and random number.
    //Processes need same llave to share memory.
    llave = ftok ("/bin/bash", 11);

    if (llave == -1)
    {
        printf( "Error getting shared memory key.\n");
        exit(0);
    }

    //Shared memory id for memory segment.
    shm_id = shmget (llave, sizeof(int)*100, 0777 | IPC_CREAT);

    if (shm_id == -1)
    {
        printf("Error getting shared memory id.\n");
        exit (0);
    }

    //Pointer to shared memory segment.
    SharedMemory = (int *)shmat (shm_id, (char *)0, 0);

    if (SharedMemory == NULL)
    {
        printf("Error gettting shared memory segment.");
        exit (0);
    }

    /* Variables compartidas de Dekker v5 */
    SharedMemory[0] = -1; // Bandera ejecución Dekker;

    SharedMemory[1] = 0; // Bandera de procedimiento 1
    SharedMemory[2] = 0; // Bandera de procedimiento 2
    SharedMemory[3] = 0; // Turno en algoritmo de Dekker


    SharedMemory[6] = 0; // Bandera primer jugador
    SharedMemory[7] = 0; // Bandera segundo jugador
    VentanaPrincipal();
        
    atexit(exit_game);
    //Free shared memory.
    shmdt ((char *)SharedMemory);
    shmctl (shm_id, IPC_RMID, (struct shmid_ds *)NULL);

    return;
}


void VentanaPrincipal(void)
{
    WINDOW * mainwin;
    if ((mainwin=initscr())==NULL)
    {
        fprintf(stderr,"Unexpected error with ncurses.");
        exit(EXIT_FAILURE);
    }
    wclear(mainwin);
    noecho();
    keypad(mainwin, TRUE);
    
    mvprintw(5,33,"SPACE INVADERS");
    mvprintw(10,35,"Bienvenido");
    mvprintw(15,26,"Presione enter para iniciar.");
    mvprintw(22,3,"Christian Marlon Renato Fabián Natareno");
    mvprintw(22,69,"201314041"); 
    refresh();

    if( getch() == 10)
    {
        wclear(mainwin);
        VentanaSeleccion();
    }
    else if( getch() == 113 || getch() == 81)
    {
        endwin();
        return;
    }
    else
    {
        wclear(mainwin);
        VentanaPrincipal();
    }
    endwin();
    return;  
}

void VentanaSeleccion(void)
{
    WINDOW * mainwin;
    if ((mainwin=initscr())==NULL)
    {
        fprintf(stderr,"Unexpected error with ncurses.");
        exit(EXIT_FAILURE);
    }
    noecho();
    keypad(mainwin, TRUE);

    mvprintw(5,26,"¿Qué jugador desea utilizar?");
    mvprintw(10,36,"Defensor");
    mvprintw(11,35,"Presione 1");
    mvprintw(14,36,"Invasor.");
    mvprintw(15,35,"Presione 2");
    char c = getch();
    if( c == 49)
    {
        wclear(mainwin);
        VentanaEspera(1);
    }
    else if( c == 50)
    {
        wclear(mainwin);
        VentanaEspera(2);
    }
    else
    {
        VentanaSeleccion();
    }
    endwin();
    return;
}


void *HiloTimer()
{    
    // SharedMemory[4] Segundos
    // SharedMemory[5] Minutos
    while(1)
    {       
        SharedMemory[4]++;
        if(SharedMemory[4] == 60) { SharedMemory[5]++; SharedMemory[4] = 0; }
        if(SharedMemory[5] == 60) { SharedMemory[5] = 0; }      
        sleep(1);       
    }
}

void VentanaEspera(int jug)
{
    int idSem; // id de Semaforo
    
    idSem = CrearSemaforos(2);
    WINDOW * mainwin;
    if ((mainwin = initscr())==NULL)
    {
        fprintf(stderr,"Unexpected error with ncurses.");
        exit(EXIT_FAILURE);
    }

    noecho();
    keypad(mainwin, TRUE);

    refresh();
    
    mvprintw(5,32,"Seleccionaste %d",jug);
    if( jug == 2 )
    {
        
        mvprintw(10,36,"Invasor.");
        mvprintw(11,21,"Espera mientras se conecta defensor...");
        refresh();

        if(SharedMemory[6] != 1)
        {
            SharedMemory[7] = 1;
            SemaforoRojo(idSem);
        }
        else
        {
            SharedMemory[7] = -1;
            SharedMemory[7] = -1;
            SemaforoVerde(idSem);
            mvprintw(12,28,"El defensor ha iniciado.");

            // Iniciar tiempo.
            // pthread_t tid;    
	        pthread_create(&tid, NULL, HiloTimer, NULL);
        }           
        Dekker_invasor();
    }
    else if( jug == 1 )
    {
        mvprintw(10,36,"Defensor");
        mvprintw(11,21,"Espera mientras se conecta invasor...");
        refresh();
        if(SharedMemory[7] != 1)
        {
            SharedMemory[6] = 1;
            SemaforoRojo(idSem);
        }
        else
        {
            SharedMemory[6] = -1;
            SharedMemory[7] = -1;
            SemaforoVerde(idSem);
            mvprintw(12,29,"El invasor ha iniciado.");

            // Iniciar tiempo.
            // pthread_t tid;    
	        pthread_create(&tid, NULL, HiloTimer, NULL);
        }
        Dekker_defensor();
    }
    endwin();
    return;
}




//SEMAFOROS
int CrearSemaforos(int n)
{
    int id;
    int llave;
    assert(n > 0); /* You need at least one! */
    llave = ftok ("/bin/bash", 12);
    id = semget(llave, n, 0600 | IPC_CREAT);
    semctl(id, 0, SETALL, 0);
    return id;
}

void BorrarSemaforos(int id)
{
    if (semctl(id, 0, IPC_RMID, NULL) == -1)
    {
        perror("Error releasing semaphore!");
        exit(EXIT_FAILURE);
    }
}

void SemaforoRojo(int id)
{
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    semop(id, &sb, 1);
}

void SemaforoVerde(int id)
{
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(id, &sb, 1);
}


//DEKKER v5
void Dekker_defensor(void)
{
    game_reset();
    redibujar_pantalla();
    accion_defensor action;
 
    while (1)
    {
        SharedMemory[1] = 1;  
        while (SharedMemory[2] == 1) 
        {
            if (SharedMemory[3] == 1) 
            {           
                SharedMemory[1] = 0;            
                while (SharedMemory[3] == (1)) {}  
                SharedMemory[1] = 1;            
            }
        }
        /* Región Critica */
        action = defensor_obtener_accion();
        if(action == DEF_SALIR)
        {
            break;
        }
        manipulacion_defensor(action);       
        if(manipulacion_balas_defensor() == 1)
        {
            SharedMemory[0] = 1;
            //break;
        }

        SharedMemory[3] = 1;  // Turno del otro proceso
        SharedMemory[1] = 0;  

        redibujar_pantalla();
        usleep(50000);
        if(SharedMemory[0] == 1)
        {
            break;
        }
    }
    endwin();
    VentanaSalida(1);
}

void Dekker_invasor(void)
{
    long contador = 0;
    game_reset();
    accion_invasor action;
    redibujar_pantalla();

    while (1)
    {
        SharedMemory[2] = 1;  
        while (SharedMemory[1] == 1) 
        {
            if (SharedMemory[3] == 0) 
            {           
                SharedMemory[2] = 0;            
                while (SharedMemory[3] == (0)) {}  
                SharedMemory[2] = 1;          
            }
        }
        /* Región Critica */
        action = invasor_obtener_accion();
        if(action == INV_SALIR)
        {
            break;
        }
        manipulacion_comandante(action);
        manipulacion_invasores(contador);
        if(manipulacion_balas_invasor() == 1)
        {
            SharedMemory[0] = 1;
            //break;
        }


        SharedMemory[3] = 0; // Turno del otro proceso
        SharedMemory[2] = 0; 

        redibujar_pantalla();
        contador++;
        usleep(50000);

        if(SharedMemory[0] == 1)
        {
            break;
        }
    }
    endwin();
    VentanaSalida(2);
}

void game_reset(void)
{
    initscr();

    SharedMemory[4] = 0; // 0 Segundos
    SharedMemory[5] = 0; // 0 Minutos

    SharedMemory[8] = 0; // Punteo Defensor

    SharedMemory[54] = 5; // Vidas invasor
    SharedMemory[55] = 5; // Vidas defensor
    
    SharedMemory[50] = PANTALLA_JUEGO_ANCHO / 2; // Posición x inicial de comandante invasor.
    SharedMemory[9] = PANTALLA_JUEGO_ANCHO / 2; // Posición x inicial de defensor.
    
    int i ;
    for (i = 0; i < 10; i++) {
        SharedMemory[10 + i] = -1; // Pos y balas defensor
        SharedMemory[20 + i] = -1; // Pos x balas defensor

        SharedMemory[30 + i] = PANTALLA_JUEGO_ALTURA + 1; // Pos y balas invasor
        SharedMemory[40 + i] = PANTALLA_JUEGO_ALTURA + 1; // Pos x balas invasor
    }

    /* INVADORES */
    SharedMemory[51] = 3; // Posicion y primer invasor
    SharedMemory[52] = 1; // Posicion x primer invasor

    for (i = 0; i < INVASORES; i++) {
        SharedMemory[56 + i] = 1; // Dar vida a invasores
    }

    noecho();
    keypad(stdscr, TRUE);
    timeout(0); /* Non-blocking mode */

}

void manipulacion_defensor(accion_defensor action)
{    
    switch (action) {
        case DEF_SALIR:           
            return;

        case DEF_DISPARAR:
            defensor_disparar();
            break;

        case DEF_MOVER_IZQ:
            SharedMemory[9]--;
            if (SharedMemory[9] < 2)
                SharedMemory[9] = 2;
            break;

        case DEF_MOVER_DER:
            SharedMemory[9]++;
            if (SharedMemory[9] > (PANTALLA_JUEGO_ANCHO - 3))
                SharedMemory[9] = PANTALLA_JUEGO_ANCHO - 3;
            break;

        case DEF_NINGUNA:
            break;
    }
}

accion_defensor defensor_obtener_accion(void)
{
    int c;
    accion_defensor action;

    action = DEF_NINGUNA; 

    while ((c = getch()) != ERR) {
        switch (c) {
            case KEY_LEFT:
                action = DEF_MOVER_IZQ;
                break;
            case KEY_RIGHT:
                action = DEF_MOVER_DER;
                break;
            case ' ':
            case KEY_UP:
                action = DEF_DISPARAR;
                break;            
            case 'q':
            case 'Q':
                return DEF_SALIR;
            default:
                break;
        }
    }
    return action;
}

void manipulacion_comandante(accion_invasor action)
{   
    switch (action) {
        case INV_SALIR:           
            return;

        case INV_MOVER_IZQ:
            SharedMemory[50]--;
            if (SharedMemory[50] < 2)
                SharedMemory[50] = 2;
            break;

        case INV_MOVER_DER:
            SharedMemory[50]++;
            if (SharedMemory[50] > (PANTALLA_JUEGO_ANCHO - 3))
                SharedMemory[50] = PANTALLA_JUEGO_ANCHO - 3;
            break;

        case INV_DISPARAR1:
            invasor_disparar(1);
            break;
            
        case INV_DISPARAR2:
            invasor_disparar(2);
            break;

        case INV_DISPARAR3:
            invasor_disparar(3);
            break;

        case INV_DISPARAR4:
            invasor_disparar(4);
            break;
            
        case INV_NINGUNA:
            break;

        default:
            break;
    }
}

accion_invasor invasor_obtener_accion(void)
{
    int c;
    accion_invasor action;

    action = INV_NINGUNA; 

    while ((c = getch()) != ERR) {
        switch (c) {
            case KEY_LEFT:
                action = INV_MOVER_IZQ;
                break;
            case KEY_RIGHT:
                action = INV_MOVER_DER;
                break;
            case '1':
                action = INV_DISPARAR1;
                break;
            case '2':
                action = INV_DISPARAR2;
                break;
            case '3':
                action = INV_DISPARAR3;
                break;
            case '4':
                action = INV_DISPARAR4;
                break;          
            case 'q':
            case 'Q':
                return INV_SALIR;
            default:
                break;
        }
    }
    return action;
}

void manipulacion_invasores(long contador)
{
    int vivos;

    vivos = INVASORES;
    if (contador % vivos != 0)
    {
        return;
    }

    if (SharedMemory[53]) 
    {
        SharedMemory[52]++;
        if (SharedMemory[52] > (PANTALLA_JUEGO_ANCHO - 46)) /* (5*5)+(4*5) */
        { 
            SharedMemory[52]--;
            //SharedMemory[51]++; // Movimiento en Y
            SharedMemory[53] = 0; // Dirección izquierda
        }
    } 
    else 
    {
        SharedMemory[52]--;
        if (SharedMemory[52] < 1) 
        {
            SharedMemory[52] = 1;
            //SharedMemory[51]++; // Movimiento en Y
            SharedMemory[53] = 1; // Dirección derecha
        }
    }
    return;
}

int manipulacion_balas_defensor(void)
{
    int x, y ;
    for (int i = 0; i < 10; i++) {
        if(SharedMemory[10 + i] >= 0)
        {
            SharedMemory[10 + i]--; // Pos y


            /* Validación colisiones con invasores */
            for (int j = 0; j < INVASORES; j++) 
            {
                if (SharedMemory[56 + j] == 1) 
                {
                    y = SharedMemory[51] + ((j / 5) * 2);
                    x = SharedMemory[52] + ((j % 5) * 10);


                    /* Colisión contra invasores fuertes */
                    if(j == 0 || j == 6 || j == 13 || j == 19) // Posiciones de naves fuertes.
                    {
                        if (SharedMemory[10 + i] == y) 
                        {
                            if ((SharedMemory[20 + i] >= x - 1) && (SharedMemory[20 + i] <= x + 5)) 
                            {
                                SharedMemory[56 + j] = 0;
                                SharedMemory[10 + i] = -1;
                                SharedMemory[8] += 15;
                                if(SharedMemory[8] >= 100)
                                {
                                    return 1;
                                }
                            }
                        }
                    }
                    /* Colisión contra naves normales */
                    else
                    {
                        if (SharedMemory[10 + i] == y) 
                        {
                            if ((SharedMemory[20 + i] >= x) && (SharedMemory[20 + i] <= x + 4)) 
                            {
                                SharedMemory[56 + j] = 0;
                                SharedMemory[10 + i] = -1;
                                SharedMemory[8] += 10;
                                if(SharedMemory[8] >= 100)
                                {
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }


            /* Validar colisiones con defensor. */
            if(SharedMemory[10 + i] == 0)
            {
                if((SharedMemory[20 + i] >= SharedMemory[50]- 2) && SharedMemory[20 + i] <= SharedMemory[50] + 2)
                {
                    SharedMemory[54]--;
                    if(SharedMemory[54] == 0)
                    {
                        return 1;
                        // Fin del juego.
                    }
                }
            } 
        }      
    }
    return 0;
}

int manipulacion_balas_invasor(void)
{
    for (int i = 0; i < 10; i++) {
        if(SharedMemory[30 + i] <= PANTALLA_JUEGO_ALTURA)
        {
            SharedMemory[30 + i]++; // Pos y

            /* Validación colisiones con defensor. */
            if(SharedMemory[30 + i] == PANTALLA_JUEGO_ALTURA)
            {
                if((SharedMemory[40 + i] >= SharedMemory[9]- 2) && SharedMemory[40 + i] <= SharedMemory[9] + 2)
                {
                    SharedMemory[55]--;
                    if(SharedMemory[55] == 0)
                    {
                        return 1;
                        // Fin del juego.
                    }
                }
            } 
        }      
    }
    return 0;
}

void redibujar_pantalla(void)
{
  
    int i,x,y, maxy, maxx;
    erase();

    /* Movimiento de balas */
    for (i = 0; i < 10; i++) {
        if(SharedMemory[10 + i] >= 0)
        {
            mvaddch(SharedMemory[10 + i], SharedMemory[20 + i] , '^');
        }

        if(SharedMemory[30 + i] >= 0)
        {
            mvaddch(SharedMemory[30 + i], SharedMemory[40 + i] , '*');
        }       
    }


    /* Pintar invasores */
    for (i = 0; i < INVASORES; i++) 
    {
        if (SharedMemory[56 + i] == 1) 
        {
            y = SharedMemory[51] + ((i / 5) * 2);
            x = SharedMemory[52] + ((i % 5) * 10);
            if(i == 0 || i == 6 || i == 13 || i == 19)
            {
                if(i == 0)
                {
                mvaddstr(y, x-1, INVASOR_FUERTE1); 
                }
                else if (i == 6)
                {
                mvaddstr(y, x-1, INVASOR_FUERTE2); 
                }
                else if (i == 13)
                {
                mvaddstr(y, x-1, INVASOR_FUERTE3);
                }
                else if (i == 19)
                {
                mvaddstr(y, x-1, INVASOR_FUERTE4);
                }                
            }
            else
            {
                mvaddstr(y, x, INVASOR_COMUN);
            }
        }
    }

    /* Pintar DEFENSOR. */
    mvaddstr(PANTALLA_JUEGO_ALTURA - 2, SharedMemory[9] - 2, DEFENSOR_1);
    mvaddstr(PANTALLA_JUEGO_ALTURA - 1, SharedMemory[9] - 2, DEFENSOR_2);

    /* Pintar COMANDANTE INVASOR .*/
    mvaddstr(0, SharedMemory[50] - 2, COMANDANTE_1);
    mvaddstr(1, SharedMemory[50] - 2, COMANDANTE_2);
  



    mvprintw(1,71,"INVASOR");
    mvprintw(2,72,"Vidas:");
    mvprintw(3,72,"%d",SharedMemory[54]);

    /* Tiempo */
    mvprintw(10,72,"Tiempo:");
    if(SharedMemory[4] <= 9)
    {
        mvprintw(11,75,"0%d", SharedMemory[4]);
    }
    else
    {
        mvprintw(11,75,"%d", SharedMemory[4]);
    }

    if(SharedMemory[5] <= 9)  
    {
        mvprintw(11,72,"0%d:", SharedMemory[5]);
    }
    else
    {
        mvprintw(11,75,"%d:", SharedMemory[5]);
    }    

    mvprintw(17,71,"DEFENSOR");
    mvprintw(18,72,"Score:");
    mvprintw(19,72,"%d",SharedMemory[8]);
    mvprintw(21,72,"Vidas:");
    mvprintw(22,72,"%d",SharedMemory[55]);

    /* Pintar Bordes */
    getmaxyx(stdscr, maxy, maxx);
    if (maxy > PANTALLA_JUEGO_ALTURA)
        mvhline(PANTALLA_JUEGO_ALTURA, 0, 0, PANTALLA_JUEGO_ANCHO + 1);
    if (maxx > PANTALLA_JUEGO_ANCHO)
        mvvline(0, PANTALLA_JUEGO_ANCHO, 0, PANTALLA_JUEGO_ALTURA);

    /* Poner cursor en DEFENSOR.*/
    move(PANTALLA_JUEGO_ALTURA - 2, SharedMemory[9]);

    refresh();
}

void defensor_disparar(void)
{
    if (SharedMemory[10] < 0)
    {
        SharedMemory[10] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[20] = SharedMemory[9];
    }
    else if (SharedMemory[11] < 0)
    {
        SharedMemory[11] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[21] = SharedMemory[9];
    }
    else if (SharedMemory[12] < 0)
    {
        SharedMemory[12] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[22] = SharedMemory[9];
    }
    else if (SharedMemory[13] < 0)
    {
        SharedMemory[13] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[23] = SharedMemory[9];
    }
    else if (SharedMemory[14] < 0)
    {
        SharedMemory[14] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[24] = SharedMemory[9];
    }
    else if (SharedMemory[15] < 0)
    {
        SharedMemory[15] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[25] = SharedMemory[9];
    }
    else if (SharedMemory[16] < 0)
    {
        SharedMemory[16] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[26] = SharedMemory[9];
    }
    else if (SharedMemory[17] < 0)
    {
        SharedMemory[17] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[27] = SharedMemory[9];
    }
    else if (SharedMemory[18] < 0)
    {
        SharedMemory[18] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[28] = SharedMemory[9];
    }
    else if (SharedMemory[19] < 0)
    {
        SharedMemory[19] = PANTALLA_JUEGO_ALTURA - 1;
        SharedMemory[29] = SharedMemory[9];
    }
    else
    {
        return; //Solo se permiten 10 balas en el campo
    }
}

void invasor_disparar(int nave)
{
    int x, y;
    x = y = -1;
    if(nave == 1 && SharedMemory[56] == 1)
    {
        y = SharedMemory[51] + ((0 / 5) * 2);
        x = SharedMemory[52] + ((0 % 5) * 10 + 2);
    }
    else if(nave == 2 && SharedMemory[62] == 1)
    {
        y = SharedMemory[51] + ((6 / 5) * 2);
        x = SharedMemory[52] + ((6 % 5) * 10 + 2);        
    }
    else if(nave == 3 && SharedMemory[69] == 1)
    {
        y = SharedMemory[51] + ((13 / 5) * 2);
        x = SharedMemory[52] + ((13 % 5) * 10 + 2);        
    }
    else if(nave == 4 && SharedMemory[75] == 1)
    {
        y = SharedMemory[51] + ((19 / 5) * 2);
        x = SharedMemory[52] + ((19 % 5) * 10 + 2);        
    }


    if (SharedMemory[30] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[30] = y;
        SharedMemory[40] = x;
    }
    else if (SharedMemory[31] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[31] = y;
        SharedMemory[41] = x;
    }
    else if (SharedMemory[32] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[32] = y;
        SharedMemory[42] = x;
    }
    else if (SharedMemory[33] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[33] = y;
        SharedMemory[43] = x;
    }
    else if (SharedMemory[34] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[34] = y;
        SharedMemory[44] = x;
    }
    else if (SharedMemory[35] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[35] = y;
        SharedMemory[45] = x;
    }
    else if (SharedMemory[36] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[36] = y;
        SharedMemory[46] = x;
    }
    else if (SharedMemory[37] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[37] = y;
        SharedMemory[47] = x;
    }
    else if (SharedMemory[38] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[38] = y;
        SharedMemory[48] = x;
    }
    else if (SharedMemory[39] > PANTALLA_JUEGO_ALTURA)
    {
        SharedMemory[39] = y;
        SharedMemory[49] = x;
    }
    else
    {
        return; //Solo se permiten 10 balas en el campo
    }
}


void VentanaSalida(int jug)
{
    pthread_cancel(tid);
    WINDOW * mainwin;
    if ((mainwin = initscr())==NULL)
    {
        fprintf(stderr,"Unexpected error with ncurses.");
        exit(EXIT_FAILURE);
    }
    wclear(mainwin);
    noecho();
    keypad(mainwin, TRUE);
    timeout(-1);
    refresh();

    mvprintw(13,33,"Tiempo ");
    if(SharedMemory[5] <= 9)  
    {
        mvprintw(13,41,"0%d:", SharedMemory[5]);
    }
    else
    {
        mvprintw(13,41,"%d:", SharedMemory[5]);
    }   

    if(SharedMemory[4] <= 9)
    {
        mvprintw(13,44,"0%d", SharedMemory[4]);
    }
    else
    {
        mvprintw(13,44,"%d", SharedMemory[4]);
    }

    if(jug == 1)
    {
        if(SharedMemory[55] <= 0)
        {
            mvprintw(2,9,"  _____          __  __ ______    ______      ________ _____ "); 
            mvprintw(3,9," / ____|   /\\   |  \\/  |  ____|  / __ \\ \\    / /  ____|  __ \\ ");
            mvprintw(4,9,"| |  __   /  \\  | \\  / | |__    | |  | \\ \\  / /| |__  | |__) |");
            mvprintw(5,9,"| | |_ | / /\\ \\ | |\\/| |  __|   | |  | |\\ \\/ / |  __| |  _  / ");
            mvprintw(6,9,"| |__| |/ ____ \\| |  | | |____  | |__| | \\  /  | |____| | \\ \\ ");
            mvprintw(7,9," \\_____/_/    \\_\\_|  |_|______|  \\____/   \\/   |______|_|  \\_\\");
                                                                            
        }
        else
        {         
            mvprintw(2,15,"__     ______  _    _  __          _______ _   _ ");
            mvprintw(3,15,"\\ \\   / / __ \\| |  | | \\ \\        / /_   _| \\ | |");
            mvprintw(4,15," \\ \\_/ / |  | | |  | |  \\ \\  /\\  / /  | | |  \\| |");
            mvprintw(5,15,"  \\   /| |  | | |  | |   \\ \\/  \\/ /   | | | . ` |");
            mvprintw(6,15,"   | | | |__| | |__| |    \\  /\\  /   _| |_| |\\  |");
            mvprintw(7,15,"   |_|  \\____/ \\____/      \\/  \\/   |_____|_| \\_|");         
        }
        mvprintw(10,36,"DEFENSOR");
        mvprintw(14,33,"Score:");
        mvprintw(14,41,"%d",SharedMemory[8]);
        mvprintw(15,33,"Vidas:");
        mvprintw(15,41,"%d",SharedMemory[55]);

    }
    else if(jug == 2)
    {        
        if(SharedMemory[54] <= 0 || SharedMemory[8] >= 100)
        {   
            mvprintw(2,9,"  _____          __  __ ______    ______      ________ _____ "); 
            mvprintw(3,9," / ____|   /\\   |  \\/  |  ____|  / __ \\ \\    / /  ____|  __ \\ ");
            mvprintw(4,9,"| |  __   /  \\  | \\  / | |__    | |  | \\ \\  / /| |__  | |__) |");
            mvprintw(5,9,"| | |_ | / /\\ \\ | |\\/| |  __|   | |  | |\\ \\/ / |  __| |  _  / ");
            mvprintw(6,9,"| |__| |/ ____ \\| |  | | |____  | |__| | \\  /  | |____| | \\ \\ ");
            mvprintw(7,9," \\_____/_/    \\_\\_|  |_|______|  \\____/   \\/   |______|_|  \\_\\");

        }
        else
        {

            mvprintw(2,15,"__     ______  _    _  __          _______ _   _ ");
            mvprintw(3,15,"\\ \\   / / __ \\| |  | | \\ \\        / /_   _| \\ | |");
            mvprintw(4,15," \\ \\_/ / |  | | |  | |  \\ \\  /\\  / /  | | |  \\| |");
            mvprintw(5,15,"  \\   /| |  | | |  | |   \\ \\/  \\/ /   | | | . ` |");
            mvprintw(6,15,"   | | | |__| | |__| |    \\  /\\  /   _| |_| |\\  |");
            mvprintw(7,15,"   |_|  \\____/ \\____/      \\/  \\/   |_____|_| \\_|");

        }
        mvprintw(10,36,"INVASOR");
        mvprintw(14,33,"Vidas:");
        mvprintw(14,41,"%d",SharedMemory[54]);
    }
   
    if( getch() == 10)
    {
        Start_SharedMemory();
    }
    else
    {
        VentanaSalida(jug);
    }
    endwin();
}



/*

Posiciones de memoria compartida.

0
1 – Bandera Proc 1
2 – Bandera Proc 2
3 – Turno Dekker
4 - Segundos
5 – Minutos
6 - Bandera primera jugador 
7 - Bandera segundo jugador
8 - Punteo de defensor
9 - Posicion de defensor
10 – 19 Pos Y balas
20 – 29 Pos X balas
30 – 39 Pos Y balas invasor
40 – 49 Pos X balas invasor
50 – posicion x de comandante invasor
51 – pos inicial invasor y
52 – pos inicial invasor x
53 – bandera direccion de invasores
54 – Vidas invasor
55 – Vidas defensor
56 – 75 Posicion invasores


*/

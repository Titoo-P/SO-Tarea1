#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> // vamos a usar una función llamada getline y ssize_t
#include <string.h>  // para usar strcmp y hacer el item 3, hacer un comando exit
#include <unistd.h>     // para fork, execvp
#include <sys/wait.h>   // para waitpid

// funcion para quitar espacios y tabs al inicio y al final
static char *trim(char *s) {
    // avanzar inicio
    while (*s == ' ' || *s == '\t') s++;

    // si quedó vacía
    if (*s == '\0') return s;

    // retroceder final
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    return s;
}




int main(void) {
    char *line = NULL;  // puntero para el getline, guardara lo que se escribe
    size_t n=0; // tamaño dinamico de lo que se escribe


    
    while(1){
        printf("mishell:$ ");
        fflush(stdout); //para obligar que se vea de una en la consola, para ver el prompt inmediatamente
        
        ssize_t r = getline(&line, &n, stdin); //guardar lo que escribe el usuario
        if(r<0){  // para verificar que si no se escribio algo pasa esto, hubo un error o se apreto ctrl + D
            putchar('\n'); //para saltar una linea
            break; //salir del bucle y terminar la shell
        }

        // Caso 1: escribe hola\n al final, aqui lo quitamos
        if (r > 0 && line[r-1] == '\n') {
            line[r - 1] = '\0'; 
        }

        // Caso 2: escribe un "enter" un espacio vacio
        if (line[0] == '\0') {
            continue;   // vuelve a mostrar el prompt
        }

        // Caso 3: detectar un exit como comando
        if (strcmp(line, "exit") == 0){
            break;
        }



        // Item 7: funcion para compatibilizar los tokens con pipes
        char *bar = strchr(line, '|');  // busca el símbolo | (que es el pipe)
        if (bar != NULL) {
            // separamos en dos procesos: izquierda (lhs) y derecha (rhs).  ej de entrada: "echo hola | tr a-z A-Z"
            *bar = '\0';           // corta la cadena en el '|'
            char *lhs = line;      // quedaria por ejemplo lado izquierdo: "echo hola"
            char *rhs = bar + 1;   // quedaria por ejemplo lado derecho:" tr a-z A-Z" 

            // limpiar espacios al borde de forma simple
            while (*lhs == ' ' || *lhs == '\t') lhs++;
            while (*rhs == ' ' || *rhs == '\t') rhs++;
            // recorta final de lhs
            if (*lhs != '\0') {
                char *end = lhs + strlen(lhs) - 1;
                while (end > lhs && (*end == ' ' || *end == '\t')) { *end = '\0'; end--; }
            }
            // recorta final de rhs
            if (*rhs != '\0') {
                char *end = rhs + strlen(rhs) - 1;
                while (end > rhs && (*end == ' ' || *end == '\t')) { *end = '\0'; end--; }
            }

            // por ahora SOLO mostramos para validar (no ejecutamos todavía)
            //printf("LHS: \"%s\"\n", lhs);
            //printf("RHS: \"%s\"\n", rhs);

            // tokenizar lado izquierdo
            char *argvL[16];  // hasta 15 args + NULL
            int argAuxL = 0;

            char *tok = strtok(lhs, " \t");
            while (tok && argAuxL < 15) {
                argvL[argAuxL++] = tok;
                tok = strtok(NULL, " \t");
            }
            argvL[argAuxL] = NULL;

            // tokenizar lado derecho
            char *argvR[16];
            int argAuxR = 0;

            tok = strtok(rhs, " \t");
            while (tok && argAuxR < 15) {
                argvR[argAuxR++] = tok;
                tok = strtok(NULL, " \t");
            }
            argvR[argAuxR] = NULL;

            // Validación: mostrar qué quedo en cada argv
            if (argAuxL == 0 || argAuxR == 0) {
                fprintf(stderr, "pipe: comandos incompletos\n");
                // volver al prompt sin ejecutar
                continue;
            }

            // ejecutar LHS | RHS 
            // crear el pipe
            int fd[2];
            if (pipe(fd) < 0) {
                perror("pipe");
                continue;
            }

            // hijo izquierdo: stdout pipe[1]
            pid_t p1 = fork();
            if (p1 < 0) {
                perror("fork");
                close(fd[0]); close(fd[1]);
                continue;
            }
            if (p1 == 0) {
                // redirigir salida estándar al extremo de escritura del pipe
                if (dup2(fd[1], STDOUT_FILENO) < 0) { perror("dup2"); _exit(127); }
                // cerrar fds que no usamos en el hijo
                close(fd[0]); close(fd[1]);
                execvp(argvL[0], argvL);
                perror("execvp LHS");
                _exit(127);
            }

            // hijo derecho: stdin pipe[0]
            pid_t p2 = fork();
            if (p2 < 0) {
                perror("fork");
                // cerrar y esperar al p1 para no dejar zombie
                close(fd[0]); close(fd[1]);
                int st; waitpid(p1, &st, 0);
                continue;
            }
            if (p2 == 0) {
                if (dup2(fd[0], STDIN_FILENO) < 0) { perror("dup2"); _exit(127); }
                close(fd[0]); close(fd[1]);
                execvp(argvR[0], argvR);
                perror("execvp RHS");
                _exit(127);
            }

            // padre: cerrar extremos y esperar a los dos
            close(fd[0]); close(fd[1]);
            int st;
            waitpid(p1, &st, 0);
            waitpid(p2, &st, 0);

            // listo: volvemos al prompt
            continue;
        }



        //Con esto separamos el texto en varias partes.
        char *argv[16]; // arreglo de maximo 16.
        int argAux = 0; // auxiliar para poner los tokens al arreglo

        char *token = strtok(line, " \t"); //seria el primer token
        while (token != NULL && argAux <15){
            argv[argAux++] = token;        // guardar token en argv
            //printf("Token %s\n", token);
            token = strtok(NULL, " \t");  //siguiente token
        }
        argv[argAux] = NULL; // ultimo siempre NULL
        


        
        if (argAux > 0) {
            pid_t pid = fork();   // crear proceso hijo
            if (pid < 0) {
                perror("fork");   // no se pudo crear hijo
                continue;
            }
            
            //Para el Item 4:
            if (pid == 0) {
                //  Proceso del hijo: reemplazar el proceso por el programa pedido
                execvp(argv[0], argv);

                // Si llegamos aqui execvp fallo (o el comando no existe o error) (devuelve -1)
                perror("execvp");          // Item 4: mostrar error
                _exit(127);                // salir del hijo con codigo tipico “command not found”
            }

            // Proceso del padre: esperar a que el hijo termine 
            int status = 0;
            if (waitpid(pid, &status, 0) < 0) { // mientras el hijo pid ejecuta el comando, el padre (shell) debe esperar
                perror("waitpid");
                // seguimos igual: mostrar de nuevo el prompt
            }
        }   



        // Si se llega aqui, "line" contiene lo que el usuario escribió
        //printf("Leiste: %s\n", line); // mostrar que se escribio

    }

    free(line);  // liberar el espacio de la memoria que getline asigno

    return 0;
}
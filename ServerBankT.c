#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFLEN 1024
#define MAXCLIENTS 10


//struct'as klientui
struct Client {
    char username[50];
    char password[5];
    float balance;
    int state;
};

//enum'as kliento busenai

enum ClientState {
    WAITING_FOR_PASSWORD,
    WAITING_FOR_OPERATION
};

struct Client clients[MAXCLIENTS];
int num_clients = 0;

//funkcija rasti klienta pagal indeksa (gaunamas slaptazodis, randamas indeksas)
int findClientIndex(const char *password) {
    for (int i = 0; i < num_clients; ++i) {
        if (strcmp(clients[i].password, password) == 0) {
            return i;
        }
    }
    return -1;
}

//funkcija, vykdanti naujo kliento inicializacija
void initializeClient(const char *password, const char *username) {
    if (num_clients < MAXCLIENTS) {
        strcpy(clients[num_clients].password, password);
        strcpy(clients[num_clients].username, username);
        clients[num_clients].balance = 0.0;
        num_clients++;
        printf("New client with password %s has been initialized with a balance of 0.\n", password);
    } else {
        printf("Cannot add new client, maximum capacity reached.\n");
    }
}

//funkcija vykdanti specifiska operacija
void handleOperation(int client_socket, int client_index, char operation, const char *username) {
    float balance = clients[client_index].balance;
    switch (operation) {
        case '1':
        //issiunciama zinute i clienta, parodanti jo balance
        char balance_msg[BUFFLEN];
        snprintf(balance_msg, sizeof(balance_msg), "Your current balance is: %.2f", clients[client_index].balance);
        if (send(client_socket, balance_msg, strlen(balance_msg), 0) < 0) {
            perror("send");
            close(client_socket);
            return;
        }
        printf("%s checked their balance\n", username);
        break;
    case '2':
    //siunciamas signalas i klienta, gavus atgal inputa panaikinamas new line
        if (send(client_socket, "How much?", sizeof("How much?"), 0) < 0) {
            perror("send");
            close(client_socket);
            return;
        }

        float deposit_amount;
        if (recv(client_socket, &deposit_amount, sizeof(float), 0) <= 0) {
            perror("recv");
            close(client_socket);
            return;
        }

        char *newline_pos = strchr((char *)&deposit_amount, '\n');
        if (newline_pos != NULL) {
            *newline_pos = '\0';
        }

			//ivykdoma operacija, issiunciamas signalas klientui kad operacija ivykdyta
        clients[client_index].balance += deposit_amount;

        printf("%s deposited %.2f into their account\n", username, deposit_amount);

        if (send(client_socket, "Deposit successful.", sizeof("Deposit successful."), 0) < 0) {
            perror("send");
            close(client_socket);
            return;
        }
        break;
    case '3':

    //siunciamas signalas i klienta, gavus atgal inputa panaikinamas new line
        if (send(client_socket, "How much?", sizeof("How much?"), 0) < 0) {
            perror("send");
            close(client_socket);
            return;
        }

    float withdraw_amount;
    if (recv(client_socket, &withdraw_amount, sizeof(float), 0) <= 0) {
        perror("recv");
        close(client_socket);
        return;
    }

	//patikrinama, ar client turi pakankamai pinigu juos isimti, jeigu ne siunciama atitinkama zinute
    if (clients[client_index].balance >= withdraw_amount) {
        clients[client_index].balance -= withdraw_amount;

        if (send(client_socket, "Withdrawal successful.", sizeof("Withdrawal successful."), 0) < 0) {
            perror("send");
            close(client_socket);
            return;
        }
        printf("%s withdrew %.2f from their account\n", username, withdraw_amount);
        } else {
            if (send(client_socket, "Insufficient balance.", sizeof("Insufficient balance."), 0) < 0) {
                perror("send");
                close(client_socket);
                return;
            }
            printf("%s did not have enough money to withdraw\n", username);
        }
        break;
    case '4':
    //uzddaromas kliento socket
        printf("%s exited the bank.\n", username);
        close(client_socket);
            break;
        default:
        //informuojamas klientas del invalid operation number
            printf("Invalid operation number.\n");
            if (send(client_socket, "Invalid operation number.", sizeof("Invalid operation number."), 0) < 0) {
                perror("send");
                close(client_socket);
                return;
            }
            break;
    }
}

//funkcija klientu handlinimui
void *handleClient(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);

    char username[BUFFLEN];
    char password[BUFFLEN];

    //gaunamas slaptazodis
    memset(password, 0, BUFFLEN);
    if (recv(client_socket, password, BUFFLEN, 0) <= 0) {
        close(client_socket);
        pthread_exit(NULL);
    }
    printf("Password: %s\n", password);

	//ieskomas klientas pagal indeksa. Jeigu tokio nera, inicializuojamas naujas ir prasoma vardo is client
    int client_index = findClientIndex(password);

    if (client_index == -1) {
        if (send(client_socket, "InputRequired2", sizeof("InputRequired2"), 0) < 0) {
            perror("send");
            close(client_socket);
            pthread_exit(NULL);
        }

        char username[BUFFLEN];
        if (recv(client_socket, username, BUFFLEN, 0) <= 0) {
            perror("recv");
            close(client_socket);
            pthread_exit(NULL);
        }
        initializeClient(password, username);
        client_index = findClientIndex(password);
        char welcome_msg[BUFFLEN];

        //issiunciamas sveikinimas
    snprintf(welcome_msg, sizeof(welcome_msg), "Welcome to the bank, %s!", clients[client_index].username);
    if (send(client_socket, welcome_msg, strlen(welcome_msg), 0) < 0) {
        perror("send");
        close(client_socket);
        pthread_exit(NULL);
    }
    } else {

    char welcome_back_msg[BUFFLEN];
    snprintf(welcome_back_msg, sizeof(welcome_back_msg), "Welcome back, %s!", clients[client_index].username);
    if (send(client_socket, welcome_back_msg, strlen(welcome_back_msg), 0) < 0) {
        perror("send");
        close(client_socket);
        pthread_exit(NULL);
    }


    }

    // Nustatoma kliento busena is enumo, kad laukiama operacijos
    clients[client_index].state = WAITING_FOR_OPERATION;

    // pagrindinis loop'as
    while (1) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(client_socket, &read_set);
        int maxfd = client_socket;

		//ziurimi laisvi socketai, jeigu toks randamas, gaunama operacija is issiunciama i handleOperation funkcija
        select(maxfd + 1, &read_set, NULL, NULL, NULL);
        if (FD_ISSET(client_socket, &read_set)) {
            char operation[BUFFLEN];
            memset(operation, 0, BUFFLEN);
            if (recv(client_socket, operation, BUFFLEN, 0) <= 0) {

                close(client_socket);
                break;
            }

            handleOperation(client_socket, client_index, operation[0], username);
        }
    }

//uzdaromas socketas ir isjungiamas threadas
    close(client_socket);
    pthread_exit(NULL);
}

//ziurima ar yra vietos naujam useriui
int findemptyuser(int c_sockets[]){
    int i;
    for (i = 0; i <  MAXCLIENTS; i++){
        if (c_sockets[i] == -1){
            return i;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    unsigned int port;
    unsigned int clientaddrlen;
    int l_socket;
    int c_sockets[MAXCLIENTS];
    fd_set read_set;

    struct sockaddr_in servaddr;
    struct sockaddr_in clientaddr;

    int maxfd = 0;
    int i;

    char buffer[BUFFLEN];

    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
        return -1;
    }
//atliekamos validacijos portui gautam ijungiant serveri
    port = atoi(argv[1]);
    if ((port < 1) || (port > 65535)) {
        fprintf(stderr, "ERROR #1: invalid port specified.\n");
        return -1;
    }

    if ((l_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "ERROR #2: cannot create listening socket.\n");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(l_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "ERROR #3: bind listening socket.\n");
        return -1;
    }

    if (listen(l_socket, 5) < 0) {
        fprintf(stderr, "ERROR #4: error in listen().\n");
        return -1;
    }

    for (i = 0; i < MAXCLIENTS; i++) {
        c_sockets[i] = -1;
    }

    while (1) {
        FD_ZERO(&read_set);
        FD_SET(l_socket, &read_set);

        for (i = 0; i < MAXCLIENTS; i++) {
            if (c_sockets[i] != -1) {
                FD_SET(c_sockets[i], &read_set);
                if (c_sockets[i] > maxfd) {
                    maxfd = c_sockets[i];
                }
            }
        }

        select(l_socket + 1, &read_set, NULL, NULL, NULL);

        if (FD_ISSET(l_socket, &read_set)) {
            int client_id = findemptyuser(c_sockets);
            if (client_id != -1) {
            	//gaunamas cliento ip adresas, kuris displayinamas
                clientaddrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, clientaddrlen);
                c_sockets[client_id] = accept(l_socket, (struct sockaddr *)&clientaddr, &clientaddrlen);
                printf("Connected:  %s\n", inet_ntoa(clientaddr.sin_addr));
					//sukuriamas naujas threadas
                pthread_t tid;
                int *client_socket_ptr = malloc(sizeof(int));
                if (client_socket_ptr == NULL) {
                    perror("malloc");
                    close(c_sockets[client_id]);
                    continue;
                }
                *client_socket_ptr = c_sockets[client_id];
                if (pthread_create(&tid, NULL, handleClient, (void *)client_socket_ptr) != 0) {
                    perror("pthread_create");
                    free(client_socket_ptr);
                    close(c_sockets[client_id]);
                    continue;
                }
                //deetachinamas threadas
                pthread_detach(tid);
            }
        }
    }

    close(l_socket);
    return 0;
}

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define BUFFLEN 1024


//password validacija

int isValidPassword(const char *password) {
    int len = strlen(password);
    if (len != 4)
        return 0;
    for (int i = 0; i < len; i++) {
        if (!isdigit(password[i]))
            return 0;
    }
    return 1;
}

//operation validacija
int isValidOperation(const char *operation) {
    if (strlen(operation) != 1)
        return 0;

    if (!isdigit(operation[0]))
        return 0;

    return 1;
}


int main(int argc, char *argv[]) {
    unsigned int port;
    int s_socket;
    struct sockaddr_in servaddr;
    fd_set read_set;

    char recvbuffer[BUFFLEN];
    char sendbuffer[BUFFLEN];
    char bank_code[20];

    int i;

    if (argc != 3) {
        fprintf(stderr, "USAGE: %s <ip> <port>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[2]);

    if ((port < 1) || (port > 65535)) {
        printf("ERROR #1: invalid port specified.\n");
        exit(1);
    }

    /*
     * Sukuriamas socket'as
     */
    if ((s_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "ERROR #2: cannot create socket.\n");
        exit(1);
    }

   /*
    * Išvaloma ir užpildoma serverio struktūra
    */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET; // nurodomas protokolas (IP)
    servaddr.sin_port = htons(port); // nurodomas portas

    /*
     * Išverčiamas simbolių eilutėje užrašytas ip į skaitinę formą ir
     * nustatomas serverio adreso struktūroje.
     */
    if (inet_aton(argv[1], &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "ERROR #3: Invalid remote IP address.\n");
        exit(1);
    }

    /*
     * Prisijungiama prie serverio
     */
    if (connect(s_socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "ERROR #4: error in connect().\n");
        exit(1);
    }

    char password[BUFFLEN];
    char operation[BUFFLEN];
    ssize_t bytes_received;

	//prasoma password, kol nera pateikiama 4 skaiciu kombinacija
     do {
        printf("Enter your 4-digit bank code: ");
        fgets(password, sizeof(password), stdin);
        password[strcspn(password, "\n")] = '\0';
    } while (!isValidPassword(password));

	//password nusiunciamas i serveri

    send(s_socket, password, strlen(password), 0);

	//laukiamas response is serverio, ar reikia user'iui pateikti varda
    memset(recvbuffer, 0, BUFFLEN);
    bytes_received = recv(s_socket, recvbuffer, BUFFLEN, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(s_socket);
        return 1;
    }


	//jeigu reikia, laukiama specifinio signalo is serverio

    recvbuffer[bytes_received] = '\0';
    if (strcmp(recvbuffer, "InputRequired2") == 0) {
        while (1) {
            char name[BUFFLEN];
            printf("This is your first time here, please enter your name: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0';

	//isiunciamas vardas i server

            send(s_socket, name, strlen(name), 0);

			//laukiamas signalas is serverio, ar reikia papildomo inputo, jeigu ne, iseinama is "if"

            memset(recvbuffer, 0, BUFFLEN);
            bytes_received = recv(s_socket, recvbuffer, BUFFLEN, 0);
            if (bytes_received < 0) {
                perror("recv");
                close(s_socket);
                return 1;
            }

            recvbuffer[bytes_received] = '\0';
            printf("%s\n", recvbuffer);

            if (strcmp(recvbuffer, "InputRequired2") != 0) {
                break;
            }
        }
    }


		//pagrindinis loop
       while (1) {

       	 	//prasoma ivesti operacijos numeri, panaikinamas new line pries issiunciant
       	do {
       	printf("Choose an operation:\n1. Check Balance\n2. Deposit\n3. Withdraw\n4. Exit\n");
        	fgets(operation, BUFFLEN, stdin);

        		if (operation[strlen(operation) - 1] == '\n') {
            	operation[strlen(operation) - 1] = '\0';
            }
        } while (!isValidOperation(operation));

			//issiunciama
        send(s_socket, operation, strlen(operation), 0);

			//laukiamas signalas is serverio, ar iseiti is while(1)
        char recvbuffer[BUFFLEN];
        memset(recvbuffer, 0, BUFFLEN);

        ssize_t bytes_received = recv(s_socket, recvbuffer, BUFFLEN, 0);
        if (bytes_received < 0) {
            perror("recv");
            close(s_socket);
            return 1;
        } else if (bytes_received == 0) {
            printf("Bank exited\n");
            close(s_socket);
            return 1;
        }

			//jeigu ne, vel laukiama signalo
        recvbuffer[bytes_received] = '\0';
        printf("%s\n", recvbuffer);

		//gavus signala is serverio, ivedamas inputas
        if (strcmp(recvbuffer, "How much?") == 0) {
            while (1) {
                float amount;
                printf("Enter amount: ");
                scanf("%f", &amount);
                while (getchar() != '\n');

					//issiunciamas inputas i serveri
                send(s_socket, &amount, sizeof(float), 0);

				//laukiama signalo, ar reikia papildomo inpu, jeigu ne, iseinama is loop
                memset(recvbuffer, 0, BUFFLEN);
                bytes_received = recv(s_socket, recvbuffer, BUFFLEN, 0);
                if (bytes_received < 0) {
                    perror("recv");
                    close(s_socket);
                    return 1;
                } else if (bytes_received == 0) {
                    printf("Bank exited\n");
                    close(s_socket);
                    return 1;
                }


                recvbuffer[bytes_received] = '\0';
                printf("%s\n", recvbuffer);

                if (strcmp(recvbuffer, "How much?") != 0) {
                    break;
                }
            }
        }

    }

//uzdaromas socket
    close(s_socket);
    return 0;
}

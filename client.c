#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORTNUM 50500

int
recv_test_from_file(char *file_path)
{
    struct sockaddr_in addr;
    int sockfd;
    /*создание сокета*/
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    /*заполнение структуры данных для подключения к серверу*/
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    /*в качестве IP адреса зарезервированная IP-константа*/
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port=htons(PORTNUM);
    
    /*подключение к серверу*/
    connect(sockfd, (struct sockaddr*) &addr, sizeof(addr));
    
    int len_file_path = htonl(strlen(file_path) + 1);
    send(sockfd, &len_file_path, sizeof(len_file_path), 0);
    
    len_file_path = ntohl(len_file_path);
    send(sockfd, file_path, len_file_path, 0);

    int mode = htonl(1);   /*режим чтения файла с сервера*/
    send(sockfd, &mode, sizeof(mode), 0);
    
    return sockfd;
}

int
send_test_to_file(char *buf, int size_buf, char *file_path)
{
    struct sockaddr_in addr;
    int sockfd;
    /*создание сокета*/
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    /*заполнение структуры данных для подключения к серверу*/
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    /*в качестве IP адреса зарезервированная IP-константа*/
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port=htons(PORTNUM);
    
    /*подключение к серверу*/
    connect(sockfd, (struct sockaddr*) &addr, sizeof(addr));

    /*отсылка размера имени файла в который сохраняем тест*/
    int len_file_path = htonl(strlen(file_path) + 1);
    send(sockfd, &len_file_path, sizeof(len_file_path), 0);

    /*отсылка имени файла в который сохраняем тест*/
    len_file_path = ntohl(len_file_path);
    send(sockfd, file_path, len_file_path, 0);

    int mode = htonl(0);   /*режим чтения файла с сервера*/
    send(sockfd, &mode, sizeof(mode), 0);
    
    /*отсылка размера строки данных*/
    size_buf = htonl(size_buf);
    send(sockfd, &size_buf, sizeof(size_buf), 0);

    /*отсылка данных серверу в виде строки-массива*/
    size_buf = ntohl(size_buf);
    send(sockfd, buf, size_buf, 0);

    int res = -1;
    /*получение ответа от сервера в виде строки*/
    recv(sockfd, &res, sizeof(res), 0);
    if (res == 0) {
        return 0;           //успешная передача
    } else {
        return -1;          //неудачная передача
    }
    
    /*закрытие сокета*/
    close(sockfd);
}

/*int
main(int argc, char *argv[])
{
    int c;
    int sockfd = recv_test_from_file("hello2.txt");
    while(read(sockfd, &c, sizeof(c)) > 0) {
        printf("%d\n", c);
    }
    printf("\n");
    //printf("%d", send_test_to_file(argv[1], strlen(argv[1]), "hello2.txt"));
}*/

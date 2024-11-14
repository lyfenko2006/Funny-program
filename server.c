#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#define PORTNUM 50500
#define QUESIZE 100 /*условный размер очереди запросов на соединение*/

int
main(int argc, char *argv[])
{
    struct sockaddr_in own_addr, /*основной сокет*/
    party_addr; /*клиентский сокет - создается после выполнения accept()*/
    int sockfd, newsockfd;
    int party_len;
    sockfd = socket(AF_INET, SOCK_STREAM, 0); /*создаем новый сокет*/
    /*заполнение данных для именования основного сокета*/
    memset(&own_addr, 0, sizeof(own_addr));
    own_addr.sin_family = AF_INET;
    /*константа INADDR_ANY обеспечивает возможность привязки сокета ко всем IP адресам компьютера, включая выделенный IP 127.0.0.1*/
    own_addr.sin_addr.s_addr = INADDR_ANY;
    /*задает порт, функция ntohs приводит 16-разрядный аргумент к сетевому порядку байт*/
    own_addr.sin_port = htons(PORTNUM);
    
    /*связываем основной сокет, приводим типа указателя к фактической структуре данных*/
    bind(sockfd, (struct sockaddr *) &own_addr, sizeof(own_addr));
    listen(sockfd, QUESIZE); /*разрешаем обработку запросов на соединение*/
    while(1) {/*основной цикл*/
        memset(&party_addr, 0, sizeof(party_addr)); /*подготавливаем очищенную структуру данных*/
        party_len = sizeof(party_addr);
        /*создание нового соединения - для каждого клиента в party_addr записывается информация о нем (IP адрес, порт)*/
        newsockfd = accept(sockfd, (struct sockaddr *)&party_addr, &party_len);
        /*работа с новым клиентом в отдельном процессе*/
        if (!fork()) {
            close(sockfd); /*этот сокет сыну не нужен*/

            int size_buf, res;
            //читаю размер имени файла и само имя файла для сохранения теста
            int size_file_path;
            recv(newsockfd, &size_file_path, sizeof(size_file_path), 0);
            size_file_path = ntohl(size_file_path);

            char *file_path = malloc(size_file_path);
            recv(newsockfd, file_path, size_file_path, 0);       /*принимаем имя файла*/

            /*чтение mode взаимодействия*/
            int mode;
            recv(newsockfd, &mode, sizeof(mode), 0);
            mode = ntohl(mode);   
    
            if (mode == 1) {
                int fd = open(file_path, O_RDONLY);
                struct stat info_f;
                stat(file_path, &info_f);
                sendfile (newsockfd, fd, 0, info_f.st_size);
            } else {

                recv(newsockfd, &size_buf, sizeof(size_buf), 0);    /*принимаем размер строки*/
                size_buf = ntohl(size_buf);
                char *buf = malloc(size_buf);
                recv(newsockfd, buf, size_buf, 0);       /*принимаем строку*/
                /*печатаем информацию о клиенте и переданную им строку*/
                printf("received string %s from client %s:%d\n", buf, inet_ntoa(party_addr.sin_addr), ntohs(party_addr.sin_port));
            
                //Мьютексы!!!
                /*записываем в файл сохраненный тест*/
                int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
                write(fd, buf, size_buf);
                close(fd);
                //Мьютексы!!!

                res = htonl(0);
                send(newsockfd, &res, sizeof(res), 0); /*посылаем клиенту ответ=0 успех*/
            }
            close(newsockfd);/*закрываем сокет*/
            return 0;
        }
        /*отец закрывает новый сокет, продолжает прослушивать старый*/
        close(newsockfd);
    }/*конец основного цикла*/
}

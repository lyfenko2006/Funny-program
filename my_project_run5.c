#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

enum
{
    COUNT_SERVICE_ARG = 5
};

//создание pid процессов
pid_t pid1, pid2, pid_check_test, fd_save_res1[2], fd_save_res2[2];
//создание каналов связи между процессами
int fd1[2], fd2[2], fd_out1[2], fd_out2[2], fd_save_test[2], fd_check_test[2];

char **
creat_my_argv(int argc, char *argv[])
{
    argc += 1;          //выделяем место под путь к программе argv[0]
    char **my_argv = calloc(argc, sizeof(char*));
        for (int i = 0; i < argc; i++) {
            my_argv[i] = calloc(100, sizeof(char*)); //добавить размер!!!
        }

        //strcpy(my_argv[0], argv[num]);
        for (int i = 1; i < argc; i++) {
            strcpy(my_argv[i], argv[i - 1]); //копируем argv без argv[0,1,2,3]
        }
    my_argv[argc] = NULL;
    return my_argv;
}

void
close_all_pipes()
{
    close(fd_check_test[0]);
    close(fd_check_test[1]);

    close(fd2[0]);
    close(fd2[1]);

    close(fd1[0]);
    close(fd1[1]);

    close(fd_out1[0]);
    close(fd_out1[1]);

    close(fd_out2[0]);
    close(fd_out2[1]);
}

int
read_result()
{
    int f_end_out_prog1 = 0, f_end_out_prog2 = 0;
    
    char res1, res2;

    pipe(fd_save_res1);
    pipe(fd_save_res2);

    while (1) {
        if (read(fd_out1[0], &res1, sizeof(res1)) != sizeof(res1)) {
            f_end_out_prog1 = 1;
        }
        write(fd_save_res1[1], &res1, sizeof(res1));
        if (read(fd_out2[0], &res2, sizeof(res2)) != sizeof(res2)) {
            f_end_out_prog2 = 1;
        }
        write(fd_save_res2[1], &res2, sizeof(res2));
        if (f_end_out_prog1 || f_end_out_prog2) {
            break;
        }
        if (res1 != res2) {
            break;
        }
    }
    
    close(fd_out2[0]);
    close(fd_out1[0]);

    close(fd_save_res1[1]);
    close(fd_save_res2[1]);

    
    if (res1 != res2 || !f_end_out_prog1 || !f_end_out_prog2) {
        return 1;                                               //вывод не совпал
    }
    return 0;                                                   //вывод совпал
}

void
print_test_error(int argc, char *argv[])
{
    unsigned char input_val;

    //printf("No\n");

    fprintf(stderr, "\n\nTest failed:\ninput:\n");
    while(read(fd_save_test[0], &input_val, sizeof(input_val) == sizeof(input_val))) {
        write(2, &input_val, sizeof(input_val));
    }

    fprintf(stderr, "\nargv: ");
    for (int i = 0; i < argc; i++) {
        fprintf(stderr, "%s ", argv[i]);
    }

    fprintf(stderr, "\nerror in byte: ");
    fprintf(stderr, "\nYour prog:\n");
    while(read(fd_save_res1[0], &input_val, sizeof(input_val) == sizeof(input_val))) {
        write(2, &input_val, sizeof(input_val));
    }

    fprintf(stderr, "\nPattern:\n");
    while(read(fd_save_res2[0], &input_val, sizeof(input_val) == sizeof(input_val))) {
        write(2, &input_val, sizeof(input_val));
    }
    fprintf(stderr, "\n\n\n");

}

void
open_pipe_link_program()
{
    pipe(fd1);                  //для передачи входных данных в 1-ую программу 
    pipe(fd2);                  //для передачи входных данных во 2-ую программу
    pipe(fd_out1);              //для передачи результата 1-ой программы
    pipe(fd_out2);              //для передачи результата 2-ой программы
    pipe(fd_save_test);         //для сохранения теста
    pipe(fd_check_test);        //для передачи программе - проверки корректности теста
}

char *
read_input(int *len_save_test)                        //ввод данных, распределение по каналам, возвращает save_test в save_test[0] лежит size_save_test
{
    unsigned char input_val;
    int size = 100;                 //начальный размер массива для сохранения ввода
    char *save_test = malloc(size);
    int size_save_test = 0;         //точный размер массива save_test
    while (read(0, &input_val, sizeof(input_val) == sizeof(input_val))) {
        if(size_save_test >= size) {
            size *= 2;
            save_test = realloc(save_test, size);
        }
        save_test[size_save_test] = input_val;
        size_save_test++;
        //разбиение входных данных на отдельные каналы для каждой программы
        write(fd1[1], &input_val, sizeof(input_val));
        write(fd2[1], &input_val, sizeof(input_val));
        write(fd_save_test[1], &input_val, sizeof(input_val));
        write(fd_check_test[1], &input_val, sizeof(input_val));
    }

    if(size_save_test + sizeof(size_save_test) >= size) {
        size *= 2;
        save_test = realloc(save_test, size);
    }

    //*(save_test + size_save_test) = size_save_test;     //???
    
    save_test = realloc(save_test, size_save_test);

    close(fd_save_test[1]);     //закрытие канала сохранения теста на запись
    close(fd_check_test[1]);    //закрытие канала передачи теста на проверку

    *len_save_test = size_save_test;

    return save_test;
}

void
save_test_in_file(char *file_path, char **my_argv, int argc, char *save_test, int size_save_test)
{
    int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    write(fd, &size_save_test, sizeof(int));
    printf("%d\n", argc);
    write(fd, &argc, sizeof(int));
    for (int i = 0; i < size_save_test; i++) {
        write(fd, &save_test[i], sizeof(save_test[i]));
    }
    for (int i = 1; i < argc + 1; i++) {
        for (int j = 0; my_argv[i][j] != '\0'; j++) {
            write(fd, &my_argv[i][j], sizeof(my_argv[i][j]));
        }
        char null_term = '\0';
        write(fd, &null_term, sizeof(char));
    }
    close(fd);
}

char **
read_file(int fd, int *size_my_argv, int *status)
{
    unsigned char input_val;
    int size_save_test;
    int argc;
    if (read(fd, &size_save_test, sizeof(size_save_test)) != sizeof(size_save_test)) {
        //fprintf(stderr, "AAA");
        *status = 0;
        return NULL;
    }
    read(fd, &argc, sizeof(argc));
    for (int i = 0; i < size_save_test; i++) {
        read(fd, &input_val, sizeof(input_val));
        //разбиение входных данных на отдельные каналы для каждой программы
        write(fd1[1], &input_val, sizeof(input_val));
        write(fd2[1], &input_val, sizeof(input_val));
        write(fd_save_test[1], &input_val, sizeof(input_val));
        //write(fd_check_test[1], &input_val, sizeof(input_val));
    }
    char **my_argv = calloc(argc + 1, sizeof(char*));
    for (int i = 0; i < argc + 1; i++) {
        my_argv[i] = calloc(100, sizeof(char*)); //добавить размер!!!
    }
    for (int i = 1; i < argc + 1; i++) {
        for (int j = 0; read(fd, &my_argv[i][j], sizeof(my_argv[i][j])); j++) {
            if (my_argv[i][j] == '\0') {
                break;
            }
        }
        //write...
    }

    close(fd_save_test[1]);     //закрытие канала сохранения теста на запись
    close(fd_check_test[1]);    //закрытие канала передачи теста на проверку

    *size_my_argv = argc;

    *status = 1;

    return my_argv;
}

int
main(int argc, char *argv[])
{
    
    int status = 1;
    int fd;

    if (strcmp(argv[3], "-t") == 0) {
        fd = open(argv[4], O_RDONLY);
    }

    while (status) {
        open_pipe_link_program();

        char *save_test;
        int size_save_test = 0;
        char flag = '0';
        char **my_argv;
        int my_argc;
        char *file_path;
        status = 0;
        if (strcmp(argv[3], "-t") == 0) {
            my_argv = read_file(fd, &my_argc, &status);
            if (!status) {
                break;
            }
            //fprintf(stderr, "%d", my_argc);
            flag = 't';
        } else {
            save_test = read_input(&size_save_test);   //считали входные данные, сохранили в save_test
            //size_save_test = save_test[size_save_test];
    
            my_argc = argc - COUNT_SERVICE_ARG;     //количество не служебных аргументов
            my_argv = creat_my_argv(my_argc, argv + COUNT_SERVICE_ARG); //создание массива дубликата argv, без служебных данных, индекс с 1
            asprintf(&file_path, "%s", argv[4]);
        }

        if (flag != 't' && (pid_check_test = fork()) == 0) {   //программа - проверка корректности теста

            dup2(fd_check_test[0], 0);          //подменяем stdin на канал
            close(fd_check_test[0]);

            //создаем массив-двойник argv без служебных аргументов

            //char **my_argv = creat_my_argv(argc, argv, 3);
            strcpy(my_argv[0], argv[3]);
            execv(argv[3], my_argv);

        } else if ((pid1 = fork()) == 0) {

            dup2(fd1[0], 0);        //подменяем stdin на канал
            dup2(fd_out1[1], 1);    //подменяем stdout на канал

            close_all_pipes();      //закрываем все каналы и на чтение и на запись

            //char **my_argv = creat_my_argv(argc, argv, 1);  //создание массива с аргументами для вызываемой программы
            strcpy(my_argv[0], argv[1]);
            //fprintf(stderr, "%s %s", my_argv[0], my_argv[1]);
            execv(argv[1], my_argv);                        //вызов программы пользователя
        } else if ((pid2 = fork()) == 0) {

            dup2(fd2[0], 0);        //подменяем stdin на канал
            dup2(fd_out2[1], 1);    //подменяем stdout на канал

            close_all_pipes();      //закрываем все каналы и на чтение и на запись

            //char **my_argv = creat_my_argv(argc, argv, 2);  //создание массива с аргументами для вызываемой программы
            strcpy(my_argv[0], argv[2]);
            execv(argv[2], my_argv);                        //вызов эталонной программы
        } else {

            close(fd_check_test[0]);
            close(fd_check_test[1]);

            close(fd1[0]);
            close(fd1[1]);

            close(fd2[0]);
            close(fd2[1]);

            close(fd_out1[1]);

            close(fd_out2[1]);

            if (flag != 't') {      //если запущено не в режиме тестирования
                //проверка теста на корректность
                int status_check_t;
                waitpid(pid_check_test, &status_check_t, 0);
                if (WEXITSTATUS(status_check_t) == 1) {
                    fprintf(stderr, "Test uncorrect\n");
                    exit(1);
                }

                //сохранение теста в файл
                save_test_in_file(file_path, my_argv, my_argc, save_test, size_save_test);
            }

            //ожидаем завершение пользовательской и эталонной программы
            wait(NULL);
            wait(NULL);

            int flag_res = read_result();       //результат совпал / не совпал

            if (flag_res) {
                if (flag != 't') {
                    print_test_error(argc, argv);         //вывод ошибочного теста
                } else {
                    my_argv[0] = "";
                    //fprintf(stderr, "%d", my_argc);
                    print_test_error(my_argc + 1, my_argv);         //вывод ошибочного теста
                }
            } else {
                printf("Yes\n");
            }

            close(fd_save_res1[0]);
            close(fd_save_res2[0]);

            close(fd_save_test[0]);
        }
    }
}   

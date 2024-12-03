#define _GNU_SOURCE
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

int send_test_to_file(char *buf, int size_buf, char *file_path);
int recv_test_from_file(char *file_path);

enum
{
    COUNT_SERVICE_ARG = 5
};

//создание каналов связи между процессами
int fd1[2], fd2[2], fd_out1[2], fd_out2[2], fd_check_test[2];

char **
creat_my_argv(int argc, char *argv[])
{
    argc += 1;          //выделяем место под путь к программе argv[0]
    char **my_argv = calloc(argc, sizeof(char*));
        for (int i = 0; i < argc; i++) {
            my_argv[i] = calloc(100, sizeof(char*)); //добавить размер!!!
        }

        for (int i = 1; i < argc; i++) {
            strcpy(my_argv[i], argv[i - 1]); //копируем argv без argv[0,1,2,3]
        }
    my_argv[argc] = NULL;
    return my_argv;
}

int
read_result(char **first_res, char **second_res, int* size_res)
{
    int f_end_out_prog1 = 0, f_end_out_prog2 = 0; /*флаги конца результата пришедшего от программ*/
    
    char res1, res2;    /*переменные для чтения результата*/

    int size = 100;
    int count = 0;
    char *res_save1 = malloc(size);     /*создание массивов для сохранения результатов*/
    char *res_save2 = malloc(size);

    while (1) {
        /*расширение массива для сохраняемого результата*/
        if (count >= size) {
            size *= 2;
            res_save1 = realloc(res_save1, size);
            res_save2 = realloc(res_save2, size);
        }

        if (read(fd_out1[0], &res1, sizeof(res1)) != sizeof(res1)) {    /*чтение байта результата 1-ой программы из канала*/
            f_end_out_prog1 = 1;
        }
        res_save1[count] = res1;
 
        if (read(fd_out2[0], &res2, sizeof(res2)) != sizeof(res2)) {    /*чтение байта результата 2-ой программы из канала*/
            f_end_out_prog2 = 1;
        }
        res_save2[count] = res2;

        count++;        /*res_save1 и res_save2 синхронизированы, поэтому один счетчик*/

        if (f_end_out_prog1 || f_end_out_prog2) {   /*сравнение окончания результата*/
            break;
        }
        if (res1 != res2) {   /*сравнение результата*/
            break;
        }
    }
    
    close(fd_out2[0]);
    close(fd_out1[0]);

    *size_res = count;      /*сохранение результатов в массивы. И размера массива результатов*/
    *first_res = res_save1;
    *second_res = res_save2;

    return (res1 != res2 || !f_end_out_prog1 || !f_end_out_prog2); //вывод совпал/не совпал
}

void
print_test_error(int argc, char *argv[], char *save_test, int size_save_test, char *first_res, char *second_res, int size_res)
{
    char input_val;
    fprintf(stderr, "\n\nTest failed:\ninput:\n");
    write(2, save_test, size_save_test);

    fprintf(stderr, "\nargv: ");
    for (int i = 0; i < argc; i++) {
        fprintf(stderr, "%s ", argv[i]);
    }

    fprintf(stderr, "\nerror in byte: ");
    fprintf(stderr, "\nYour prog:\n");
    write(2, first_res, size_res * sizeof(first_res[0]));

    fprintf(stderr, "\nPattern:\n");

    write(2, second_res, size_res * sizeof(second_res[0]));

    fprintf(stderr, "\n\n\n");

}

void
check_res(int argc, char **argv, char *save_test, int size_save_test)
{
    char *first_res, *second_res;
    int size_res;
    if (read_result(&first_res, &second_res, &size_res) == 1) {
        print_test_error(argc, argv, save_test, size_save_test, first_res, second_res, size_res);         //вывод ошибочного теста
    } else {
        printf("Yes\n");
    }
}

void
open_pipe_link_program()
{
    pipe2(fd1, O_CLOEXEC);                  //для передачи входных данных в 1-ую программу 
    pipe2(fd2, O_CLOEXEC);                  //для передачи входных данных во 2-ую программу
    pipe2(fd_out1, O_CLOEXEC);              //для передачи результата 1-ой программы
    pipe2(fd_out2, O_CLOEXEC);              //для передачи результата 2-ой программы
    pipe2(fd_check_test, O_CLOEXEC);        //для передачи программе - проверки корректности теста
}

char *
read_input(int *len_save_test)
{
    unsigned char input_val;

    int size = 100;                 //начальный размер массива для сохранения ввода
    char *save_test = malloc(size);
    int size_save_test = 0;         //точный размер массива save_test

    while (read(0, &input_val, sizeof(input_val) == sizeof(input_val))) {
        if(size_save_test >= size) {            /*расширение массива для сохранения теста*/
            size *= 2;
            save_test = realloc(save_test, size);
        }
        save_test[size_save_test] = input_val;
        size_save_test++;
        //разбиение входных данных на отдельные каналы для каждой программы
        write(fd1[1], &input_val, sizeof(input_val));
        write(fd2[1], &input_val, sizeof(input_val));
        write(fd_check_test[1], &input_val, sizeof(input_val));
    }

    if(size_save_test + sizeof(size_save_test) >= size) {   /*расширение массива для сохранения теста*/
        size *= 2;
        save_test = realloc(save_test, size);
    }
    
    save_test = realloc(save_test, size_save_test);     /*урезание массива до нужного размера*/

    close(fd_check_test[1]);    //закрытие канала передачи теста на проверку

    *len_save_test = size_save_test;

    return save_test;
}

long long
total_size(char **arr1, int size_arr1, char *arr2, int size_arr2) /*возможно нужно добавить к размеру число строк argv*/
{
    long long res = 0;
    res += sizeof(size_arr1);
    for (int i = 0; i < size_arr1; i++) {
        res += strlen(arr1[i]) + 1;
    }
    res += sizeof(size_arr2);
    res += strlen(arr2) + 1;
    return res;
}

void
save_test_in_file(char *file_path, char **my_argv, int argc, char *save_test, int size_save_test)/*strcat с разделителями!*/
{
    long long size = total_size(my_argv, argc, save_test, size_save_test);
    char *buf = malloc(size);      /*строка, которая будет записана в файл*/

    char *temp = (char *) &size_save_test;         /*запись размера input в строку*/
    for (int i = 0; i < sizeof(size_save_test); i++) {
        buf[i] = temp[i];
    }

    temp = (char *) &argc;              /*запись размера argv в строку*/
    for (int i = sizeof(size_save_test); i < sizeof(argc) + sizeof(size_save_test); i++) {
        buf[i] = temp[i - sizeof(size_save_test)];
    }

    int size_num = sizeof(size_save_test) + sizeof(argc);       /*запись input в строку*/
    for (int i = size_num; i < size_save_test + size_num; i++) {
        buf[i] = save_test[i - size_num];
    }

    buf[size_save_test + size_num] = '\0';

    for (int i = 1; i < argc + 1; i++) {            /*запись argv без служебных аргументов в строку*/
        strcat(buf + size_num + size_save_test, my_argv[i]);
        strcat(buf + size_num + size_save_test, "\n");
    }

    if (send_test_to_file(buf, size, file_path) != 0) { /*отправка строки в файл*/
        fprintf(stderr, "error writing to file\n");
        exit(1);
    }
}

char **
read_file(int fd, int *size_my_argv, int *status, char **save_test, int *size_save_test)
{
    unsigned char input_val;
    int size_test;
    
    int argc;

    if (read(fd, &size_test, sizeof(size_test)) != sizeof(size_test)) {  /*чтение размера input*/
        *status = 0;
        return NULL;
    }

    char *my_save_test = calloc(size_test, sizeof(unsigned char));  /*массив для сохраненных тестов*/
    int count = 0;

    read(fd, &argc, sizeof(argc));      /*чтение размера argv*/

    for (int i = 0; i < size_test; i++) {  /*чтение input*/
        read(fd, &input_val, sizeof(input_val));
        my_save_test[count] = input_val;
        count++;
        //разбиение входных данных на отдельные каналы для каждой программы
        write(fd1[1], &input_val, sizeof(input_val));
        write(fd2[1], &input_val, sizeof(input_val));
    }
    char **my_argv = calloc(argc + 2, sizeof(char*));       /*создание и заполнение массива с аргументами командной строки*/
    for (int i = 0; i < argc + 1; i++) {
        my_argv[i] = calloc(100, sizeof(char*)); //добавить размер!!!
    }
    for (int i = 1; i < argc + 1; i++) {
        for (int j = 0; read(fd, &my_argv[i][j], sizeof(my_argv[i][j])); j++) {
            if (my_argv[i][j] == '\n') {
                my_argv[i][j] = '\0';
                break;
            }
        }
    }
    my_argv[argc + 1] = NULL;
    close(fd_check_test[1]);    //закрытие канала передачи теста на проверку

    *size_my_argv = argc;
    *size_save_test = size_test;    /*возвращаем массив input и размер input*/
    *save_test = my_save_test;

    *status = 1;

    return my_argv;
}

void
all_close(int *fd)
{
    close(fd[0]);
    close(fd[1]);
}

int
start_prog(int *fd_in, int *fd_out, char *name_prog, char **argv)
{
    pid_t pid;
    if ((pid = fork()) == 0) {
        if (fd_in) {
            dup2(fd_in[0], 0);        //подменяем stdin на канал
        }
        if (fd_out) {
            dup2(fd_out[1], 1);    //подменяем stdout на канал
        }
        strcpy(argv[0], name_prog);
        execv(name_prog, argv);
        fprintf(stderr, "No file %s\n", name_prog);
        exit(0);
    }
    return pid;
}

int
main(int argc, char *argv[])
{
    
    int status = 1;
    int fd;
    char **my_argv;
    int my_argc;
    char *save_test;
    int size_save_test = 0;
    char *file_path;

    if (strcmp(argv[3], "-t") == 0) {
        fd = recv_test_from_file(argv[4]);
        while (status) {
            open_pipe_link_program();
            status = 0;
            my_argv = read_file(fd, &my_argc, &status, &save_test, &size_save_test);
            if (!status) {
                break;
            }

            start_prog(fd1, fd_out1, argv[1], my_argv);
            start_prog(fd2, fd_out2, argv[2], my_argv);

            all_close(fd_check_test);
            all_close(fd1);
            all_close(fd2);
            close(fd_out1[1]);
            close(fd_out2[1]);

            strcpy(my_argv[0], "");
            check_res(my_argc + 1, my_argv, save_test, size_save_test);         //вывод ошибочного теста

            while(wait(NULL) > 0) {}
        }
    } else {
        open_pipe_link_program();
        save_test = read_input(&size_save_test);   //считали входные данные, сохранили в save_test
    
        my_argc = argc - COUNT_SERVICE_ARG;     //количество не служебных аргументов
        my_argv = creat_my_argv(my_argc, argv + COUNT_SERVICE_ARG); //создание массива дубликата argv, без служебных данных, индекс с 1
        asprintf(&file_path, "%s", argv[4]);

        int pid_check_test = start_prog(fd_check_test, NULL, argv[3], my_argv);
        start_prog(fd1, fd_out1, argv[1], my_argv);
        start_prog(fd2, fd_out2, argv[2], my_argv);

        all_close(fd_check_test);
        all_close(fd1);
        all_close(fd2);
        close(fd_out1[1]);
        close(fd_out2[1]);

        //проверка теста на корректность
        int status_check_t;
        waitpid(pid_check_test, &status_check_t, 0);
        if (WIFEXITED(status_check_t) && WEXITSTATUS(status_check_t) == 1) {
            fprintf(stderr, "Test uncorrect\n");
            exit(1);
        }

        //сохранение теста в файл
        save_test_in_file(file_path, my_argv, my_argc, save_test, size_save_test);

        check_res(argc, argv, save_test, size_save_test);

        while(wait(NULL) > 0) {}
    }
}   

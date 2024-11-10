#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

//создание pid процессов
pid_t pid1, pid2, pid_check_test, fd_save_res1[2], fd_save_res2[2];
//создание каналов связи между процессами
int fd1[2], fd2[2], fd_out1[2], fd_out2[2], fd_save_test[2], fd_check_test[2];

char **
creat_my_argv(int argc, char *argv[], int num)
{
    char **my_argv = calloc(argc, sizeof(char*));
        for (int i = 0; i < argc; i++) {
            my_argv[i] = calloc(100, sizeof(char*)); //добавить размер!!!
        }

        strcpy(my_argv[0], argv[num]);
        for (int i = 3; i < argc - 1; i++) {
            strcpy(my_argv[i - 2], argv[i + 1]); //копируем argv без argv[0,1,2,3]
        }
    my_argv[argc - 3] = NULL;
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

    printf("No\n");

    fprintf(stderr, "Test failed:\ninput:\n");
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
    fprintf(stderr, "\n");

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

void
read_input()
{
    unsigned char input_val;
    while (read(0, &input_val, sizeof(input_val) == sizeof(input_val))) {
        //разбиение входных данных на отдельные каналы для каждой программы
        write(fd1[1], &input_val, sizeof(input_val));
        write(fd2[1], &input_val, sizeof(input_val));
        write(fd_save_test[1], &input_val, sizeof(input_val));
        write(fd_check_test[1], &input_val, sizeof(input_val));
    }

    close(fd_save_test[1]);     //закрытие канала сохранения теста на запись
    close(fd_check_test[1]);    //закрытие канала передачи теста на проверку
}

int
main(int argc, char *argv[])
{
    
    open_pipe_link_program();

    read_input();
    
    if ((pid_check_test = fork()) == 0) {   //программа - проверка корректности теста
        
        dup2(fd_check_test[0], 0);          //подменяем stdin на канал
        close(fd_check_test[0]);

        //создаем массив-двойник argv без служебных аргументов
        
        char **my_argv = creat_my_argv(argc, argv, 3);
        execv(argv[3], my_argv);

    } else if ((pid1 = fork()) == 0) {

        dup2(fd1[0], 0);        //подменяем stdin на канал
        dup2(fd_out1[1], 1);    //подменяем stdout на канал
        
        close_all_pipes();      //закрываем все каналы и на чтение и на запись

        char **my_argv = creat_my_argv(argc, argv, 1);  //создание массива с аргументами для вызываемой программы

        execv(argv[1], my_argv);                        //вызов программы пользователя
    } else if ((pid2 = fork()) == 0) {
        
        dup2(fd2[0], 0);        //подменяем stdin на канал
        dup2(fd_out2[1], 1);    //подменяем stdout на канал
        
        close_all_pipes();      //закрываем все каналы и на чтение и на запись

        char **my_argv = creat_my_argv(argc, argv, 2);  //создание массива с аргументами для вызываемой программы

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

        //проверка теста на корректность
        int status_check_t;
        waitpid(pid_check_test, &status_check_t, 0);
        if (WEXITSTATUS(status_check_t) == 1) {
            fprintf(stderr, "Test uncorrect\n");
            exit(1);
        }
        
        //ожидаем завершение пользовательской и эталонной программы
        wait(NULL);
        wait(NULL);
        
        int flag = read_result();       //результат совпал / не совпал

        if (flag) {
            print_test_error(argc, argv);         //вывод ошибочного теста
        } else {
            printf("Yes\n");
        }

        close(fd_save_res1[0]);
        close(fd_save_res2[0]);

        close(fd_save_test[0]);
    }
}

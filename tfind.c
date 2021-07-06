/* #################################################### */
/*                    21600786                          */
/*                      홍승훈                            */
/* #################################################### */
/*           ./tfind [<option>] <dir> [<keyword>]       */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/file.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

/*QUEUE STRUCTURE && FUNCTIONS*/
typedef struct __node_t
{
    char dir[300];
    struct __node_t *next;
} node_t;

typedef struct __queue_t
{
    node_t *head;
    node_t *tail;
    pthread_mutex_t headLock, tailLock;
} queue_t;

void Queue_Init(queue_t *q)
{
    node_t *tmp = malloc(sizeof(node_t));
    tmp->next = NULL;
    q->head = q->tail = tmp;
    pthread_mutex_init(&q->headLock, NULL);
    pthread_mutex_init(&q->tailLock, NULL);
}

void Queue_Enqueue(queue_t *q, char *inputdir)
{
    char dir[300];
    memcpy(dir, inputdir, sizeof(dir));
    node_t *tmp = malloc(sizeof(node_t));
    memcpy(tmp->dir, dir, sizeof(dir));
    tmp->next = NULL;
    printf("Enqueue test: %s\n", tmp->dir);

    pthread_mutex_lock(&q->tailLock);
    q->tail->next = tmp;
    q->tail = tmp;
    pthread_mutex_unlock(&q->tailLock);
}

char *Queue_Dequeue(queue_t *q)
{
    pthread_mutex_lock(&q->headLock);
    char *dir = malloc(sizeof(char) * 300);
    node_t *tmp = q->head;
    node_t *newHead = tmp->next;
    if (newHead == NULL)
    {
        pthread_mutex_unlock(&q->headLock);
        return NULL;
    }
    q->head = newHead;
    pthread_mutex_unlock(&q->headLock);
    memcpy(dir, tmp->dir, sizeof(tmp->dir));
    free(tmp);
    return dir;
}

/*GLOBAL VARIABLE*/
int quit_flag = 0;
int filecount = 0;
int linecount = 0;
clock_t start, end;
int counts = 0;
char **keyword;
int pipes_0[2];
int pipes_1[2];
char *wherefile = "/usr/bin/file";
char *Asc = "ASCII";
pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t filecount_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t linecount_lock = PTHREAD_MUTEX_INITIALIZER;
queue_t q;

void *worker() //counts 가 0이면 그냥 끝내(lock해서 읽어와)
{
    //If all directoroies are read, Just return 0, It meas there is no readable in Queue.
    pthread_mutex_lock(&count_lock);
    if (counts <= 0)
    {
        pthread_mutex_unlock(&count_lock);
        return (void *)0;
    }
    pthread_mutex_unlock(&count_lock);

    DIR *dp = NULL;
    struct dirent *file = NULL;
    char list[100][300];
    char directory[300];
    char Wrt[10000];
    char buf[1024];
    int i = 0;
    int numbers;
    FILE *check = NULL;
    char line[1024];

    memcpy(directory, Queue_Dequeue(&q), sizeof(directory));
    if ((dp = opendir(directory)) == NULL) /*opendir*/
    {
        return (void *)0;
    }
    for (int i = 0; (file = readdir(dp)) != NULL; i++) 
    {
        if (strcmp(file->d_name, ".") && strcmp(file->d_name, "..") && (file->d_type == 8))
        {
            sprintf(list[i], "file %s/%s", directory, file->d_name);
            //printf("file name test %s\n ", list[i]);
            if ((check = popen(list[i], "r")) == NULL)
                return (void *)0;
            if (fgets(buf, 1024, check) == NULL)
            {
                perror("failed to read buffer\n");
                exit(1);
            }
            if (strstr(buf, Asc) != NULL)
            {
                pthread_mutex_lock(&filecount_lock);
                filecount++;
                pthread_mutex_unlock(&filecount_lock);
                int line = 1;
                char path[1024];
                char str[1024];
                strcpy(path, directory);
                strcat(path, "/");
                strcat(path, file->d_name);
                FILE *fd = fopen(path, "r");
                if (fd == NULL)
                {
                    perror("Error to open regular file.\n");
                    exit(1);
                }
                while (!feof(fd))
                {
                    int all_keyword_check = 1;
                    char *p = fgets(str, 1024, fd);
                    for (int i = 0; keyword[i] != NULL; i++)
                    {
                        if (!strstr(str, keyword[i]))
                            all_keyword_check = all_keyword_check * 0;
                    }
                    if (all_keyword_check)
                    { 
                        printf("%s:%d:%s\n", path, line, str);
                        pthread_mutex_lock(&linecount_lock);
                        linecount++;
                        pthread_mutex_unlock(&linecount_lock);
                    }
                    line++;
                }
                fclose(fd);

            }
        }
    }
    closedir(dp);
    return (void *)(-1);
}

void report(int filecount, int linecount, float exe_time){
    printf("\n\n========================================\n");
    printf("TOTAL LINES : %d\n", linecount);
    printf("TOTAL EXPLORED FILES : %d\n", filecount);
    printf("TOTAL EXCUTION TIME : %fsec\n", exe_time);
    return;
}



int main(int argc, char *argv[])
{
    start = clock();
    printf("===============================================\n");
    Queue_Init(&q);
    char *directory;
    int num = 0;
    int opt;
    extern char *optarg;
    extern int optind;
    char subdir[500][300];
    char *wheretree = "/usr/bin/tree";

    //OPTION ARGUMENT
    while ((opt = getopt(argc, argv, "t:")) != -1)
    {
        switch (opt)
        {
        case 't':
            num = atoi(optarg);
            if (num < 1 || num > 16)
            {
                exit(1);
            }
            break;
        case '?':
            exit(1);
        }
    }
    //DIRECTORY INPUT CHECK
    if ((directory = argv[optind]) == NULL)
        exit(1);
    //KEYWORD INPUT CHECK
    if (argc - optind > 9 || argc - optind == 1)
        exit(1);

    //IDNETIFY KEYWORD
    optind++;
    keyword = (char **)malloc(sizeof(char *) * (argc - optind));
    for (int i = 0; i < argc - optind; i++)
    {
        keyword[i] = (char *)malloc(sizeof(char) * strlen(argv[optind + i]));
    }
    for (int i = 0; i < argc - optind; i++)
    {
        keyword[i] = argv[optind + i];
        printf("INPUT KEYWORDS: \"%s\" \n", keyword[i]);
    }

    printf("INPUT DIRECTORY:  \"%s\" \n", directory);
    printf("NUMBER OF THREAD: \"%d\" \n", num);
    printf("===============================================\n\n");

    //TREE FUNCTION -> FIND ALL SUBDIRECTORY
    pid_t tree;
    if (pipe(pipes_1) != 0)
    {
        perror("Error");
        exit(1);
    }
    tree = fork();
    if (tree == 0) //child  -> execl tree -> stdout(dup2) ->  받아와
    {
        dup2(pipes_1[1], 1); /* standard output*/
        execl(wheretree, "tree", directory, "-d", "-fi", (char *)NULL);
    }
    else
    {
        char buf[1000001];
        ssize_t s;
        close(pipes_1[1]); /* close standard output*/
        while ((s = read(pipes_1[0], buf, 1000000)) > 0)
            buf[s + 1] = 0x0;
        char *ptr = strtok(buf, "\n");
        while (ptr != NULL)
        {
            strcpy(subdir[counts], ptr);
            ptr = strtok(NULL, "\n");
            counts++;
        }
        wait(NULL);
    }
    char exx[300];
    for (int i = 1; i <= counts - 1; i++)
    {
        for (int j = 0; j < counts - i; j++)
        {
            if (strlen(subdir[j]) > strlen(subdir[j + 1]))
            {
                strcpy(exx, subdir[j + 1]);
                strcpy(subdir[j + 1], subdir[j]);
                strcpy(subdir[j], exx);
            }
        }
    }
    printf("DIRECTORY LIST: \n");
    for (int i = 1; i < counts; i++)
    {
        Queue_Enqueue(&q, subdir[i]);
    }
    printf("===============================================\n\n");

    pthread_t p;
    int temp = 0;
    Queue_Dequeue(&q);
    printf("Start count = %d \n", counts);
    while(counts>0)
    {
        for(int i = 0; i< num; i++)
        {
            pthread_create(&p, NULL, worker, (void *)NULL);
        }
        for(int i = 0; i< num; i++)
        {
            pthread_join(p, (void **)&temp);
            printf("return = %d\n", (int)temp);
            counts = counts + (int)temp;
            printf("Mid count = %d \n", counts);
            if(counts<0)
            {
                end = clock();
                report(filecount, linecount ,(float) (end - start)/CLOCKS_PER_SEC);
                quit_flag=1;
                break;
            }
            printf("Last count = %d \n", counts);
        }
            

        if(quit_flag==1)
            break;
    }
    //pthread_create(&p, NULL, worker, (void *)NULL);
    //pthread_join(p, (void **)&temp);
    printf("check = %d\n", counts);
}
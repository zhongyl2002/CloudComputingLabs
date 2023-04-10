#include<stdio.h>
#include<iostream>
#include<sys/sysinfo.h>
#include<pthread.h>
#include<unistd.h>
#include<fcntl.h>
#include<cstring>
#include<cstdlib>
#include<cassert>
#include<stdbool.h>

#include"sudoku.h"

#define debug true
#define BATCH_SIZE 512 * 4;

using namespace std;

const int problemNum = 10000,
          workPerThread = 50,
          files = 8,
          fileChars = 100;

struct queue
{
    pthread_t pid;
    pthread_cond_t queueFull;
    pthread_mutex_t queueMutex;
    int* todoWorkList;
    int todoWorkNum, todoWorkBegin, id;

    queue()
    {
        pthread_cond_init(&queueFull, NULL);
        pthread_mutex_init(&queueMutex, NULL);
        todoWorkList = (int*)malloc(workPerThread * sizeof(int));
        todoWorkNum = 0, todoWorkBegin = 0;
        if(debug) printf("queue init ok\n");
        fflush(stdout);
    }
    ~queue()
    {
        pthread_cond_destroy(&queueFull);
        pthread_mutex_destroy(&queueMutex);
        free(todoWorkList);
    }
};

// 文件相关变量
char fileNameBuffer[files][fileChars];
char fileName[fileChars];
int fileBegin, 
    fileEnd;
bool fileOver = false;

pthread_mutex_t fileMutex;
pthread_cond_t fileFull, 
               fileEmpty;


// 问题数据暂存
char problem[problemNum][128], 
     problemStatus[problemNum];
int problemBegin, 
    problemEnd, 
    problemTotal, 
    problemSolved;


// 任务分配
int problemDistributeBegin;


// 线程pid
pthread_t getFNT, 
          readFT, 
          printRT, 
          testT, 
          distributeT;
pthread_t* workerT;

pthread_mutex_t problemMutex;
pthread_cond_t problemFull, 
               problemEmpty;

pthread_mutex_t workerMutex;
pthread_cond_t workerFull, 
               workerEmpty;


// 配置数据
int cores, 
    threadNum;
queue* threadQueue;

void* getFileName(void* args)
{
    if(debug) printf("in getFileNameT\n");
        fflush(stdout);
    // TODO:改善读入方法
    while(scanf("%s", fileName) != EOF)
    {
        if(debug) printf("in getFileNameT while\n");
        fflush(stdout);
        pthread_mutex_lock(&fileMutex);
        while ((fileEnd + 1) % files == fileBegin)
        {
            if(debug) printf("getFileNameT wait\n");
                fflush(stdout);
            pthread_cond_wait(&fileEmpty, &fileMutex);
        }
        strncpy(fileNameBuffer[fileEnd], fileName, fileChars - 1);
        if(debug)printf("getFNT:\treceive file[%d]:%s\n", fileEnd, fileNameBuffer[fileEnd]);
        fflush(stdout);
        fileEnd = (fileEnd + 1) % files;

        pthread_cond_broadcast(&fileFull);
        pthread_mutex_unlock(&fileMutex);
    }
    pthread_mutex_lock(&fileMutex);
    fileOver = true;
    pthread_mutex_unlock(&fileMutex);
    return NULL;
}

void* readFile(void* args)
{
    if(debug)printf("in readFile Thread!\n");
        fflush(stdout);
    while (1)
    {
        if(debug)printf("in readFile Thread while!\n");
        fflush(stdout);
        pthread_mutex_lock(&fileMutex);
        if(fileOver)
        {
            pthread_mutex_unlock(&fileMutex);
            break;
        }
        while(fileBegin == fileEnd)
        {
            if(debug)printf("readFileT waiting!\n");
            printf("fileOver = %d\n", fileOver);
                fflush(stdout);
            pthread_cond_wait(&fileFull, &fileMutex);
        }

        // 打开并读取文件
        int fd = open(fileNameBuffer[fileBegin], O_RDONLY);

        assert(fd != -1 && "file open failed!\n");

        pthread_mutex_lock(&problemMutex);
        while(true)
        {
            // 读文件每行末尾的'\n'
            // int tmp = read(fd, &endLine, 1);
            printf("pb = %d, pe = %d, tmp = %d, judge = %d\n", 
                    problemBegin, problemEnd, 
                    (problemEnd + 1) % problemNum, problemBegin == ((problemEnd + 1) % problemNum));
            while((problemEnd + 1) % problemNum == problemBegin);   // 问题满
            {
                printf("PB full\n");fflush(stdout);
                pthread_cond_wait(&problemEmpty, &problemMutex);
            }
            printf("Hello\n");fflush(stdout);
            if(read(fd, problem[problemEnd], 82) > 0 
                && problem[problemBegin][0] != '\n')
            {
                if(debug)printf("readFT:\treceive problem %d\n", problemEnd);
                    fflush(stdout);
                for(int i = 0; i < strlen(problem[problemEnd]); i ++)
                    printf("%c", problem[problemEnd][i]);
                printf("\n");
                fflush(stdout);
                problemEnd = (problemEnd + 1) % problemNum;
                problemTotal ++;
                pthread_cond_broadcast(&problemFull);
            }
            else
            {
                break;
            }
        }
        pthread_mutex_unlock(&problemMutex);

        fileBegin = (++fileBegin) % files;

        pthread_cond_broadcast(&fileEmpty);
        pthread_mutex_unlock(&fileMutex);
    }
    return NULL;
}

void* distribute(void* args)
{
    
    return NULL;
}

/*
功能：使用solve_sudoku_dancing_links算法求解数独
输入：数独问题在全局数组中的位置、编号
输出：修改全局数组中的各个问题是否被解决标志位
*/
void* worker(void* args)
{
    int id = *(int*)args;
    if(debug) printf("in worker thread\n");
        fflush(stdout);
    init_neighbors();
    while(1)
    {
        if(debug) printf("in worker thread while\n");
        fflush(stdout);
        pthread_mutex_lock(&threadQueue[id].queueMutex);
        while(threadQueue[id].todoWorkNum == 0)
        {
            if(debug) printf("in worker thread wait\n");
        fflush(stdout);
            pthread_cond_wait(&threadQueue[id].queueFull, &threadQueue[id].queueMutex);
        }
        int pos = threadQueue[id].todoWorkBegin;
        int problemPos = threadQueue[id].todoWorkList[pos];
        if(debug) printf("woker[%d]:get problem %d\n", id, problemPos);
        fflush(stdout);
        input(problem[problemPos]);
        solve_sudoku_dancing_links(problem[problemPos]);
        threadQueue[id].todoWorkBegin = (threadQueue[id].todoWorkBegin + 1) % workPerThread;
        threadQueue[id].todoWorkNum --;
        pthread_mutex_unlock(&threadQueue[id].queueMutex);

        pthread_mutex_lock(&problemMutex);
        problemSolved ++;
        pthread_mutex_unlock(&problemMutex);
    }
    if(debug) printf("worker over!\n");
    fflush(stdout);
    return NULL;
}

void init()
{
    threadNum = cores;

    pthread_cond_init(&fileFull, NULL);
    pthread_cond_init(&fileEmpty, NULL);
    pthread_mutex_init(&fileMutex, NULL);

    pthread_cond_init(&problemFull, NULL);
    pthread_cond_init(&problemEmpty, NULL);
    pthread_mutex_init(&problemMutex, NULL);

    pthread_cond_init(&workerFull, NULL);
    pthread_cond_init(&workerEmpty, NULL);
    pthread_mutex_init(&workerMutex, NULL);

    threadQueue = (queue*)malloc(threadNum * sizeof(queue));
    
    if(debug) printf("init function OK\n");
        fflush(stdout);
}

void waitForAllJobsDone()
{
    //Lazily wait all the worker threads finish their wget jobs
    while(1)
    {
        if(debug)printf("waitT:\tin waitT!\n");
        fflush(stdout);
        usleep(10000);//Check per 10 ms
        pthread_mutex_lock(&fileMutex);
        if(problemTotal == problemSolved && problemTotal != 0)//All jobs done
        {
            pthread_mutex_unlock(&fileMutex);
            break;
        }
        pthread_mutex_unlock(&fileMutex);
    }
}

int main()
{
    cores = get_nprocs();
    if(debug)
        printf("Available cores in the machine : %d\n", cores);
        fflush(stdout);
    fflush(stdout);

    init();

    if(pthread_create(&getFNT, NULL, getFileName, NULL) != 0)
    {
        printf("fail to create get-file-name-thread!\n");
        fflush(stdout);
        exit(1);
    }

    if(pthread_create(&readFT, NULL, readFile, NULL) != 0)
    {
        printf("fail to create read-file-thread!\n");
        fflush(stdout);
        exit(1);
    }

/*
    if(pthread_create(&distributeT, NULL, distribute, NULL) != 0)
    {
        printf("fail to create distributeT\n");
        fflush(stdout);
        exit(1);
    }
*/
/*
    for(int i = 0; i < threadNum; i ++)
    {
        threadQueue[i].id = i;
        if(pthread_create(&threadQueue[i].pid, NULL, worker, &threadQueue[i].id) != 0)
        {
            printf("fail to create workerT : %d\n", i);
        fflush(stdout);
            exit(1);
        }
    }
*/
    pthread_join(getFNT, NULL);
    pthread_join(readFT, NULL);
    // waitForAllJobsDone();

    if(debug) printf("main over!\n");

    return 0;
}
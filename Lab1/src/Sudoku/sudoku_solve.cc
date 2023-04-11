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

#include"mySudoku.hpp"

#define debug false
#define BATCH_SIZE 512 * 4;

using namespace std;

const int problemNum = 1000000,
          workPerThread = 50,
          files = 8,
          fileChars = 100, 
          boardSize = 81;

struct queue
{
    pthread_t pid;
    pthread_cond_t queueFull;
    pthread_mutex_t queueMutex;
    int* todoWorkList;
    int todoWorkBegin, todoWorkEnd, id;
    int* qboard;
};

void queueNodeInit(queue& tmp)
{
    pthread_cond_init(&tmp.queueFull, NULL);
    pthread_mutex_init(&tmp.queueMutex, NULL);
    tmp.todoWorkList = (int*)malloc(workPerThread * sizeof(int));
    tmp.qboard = (int*)malloc(boardSize * sizeof(int));
    tmp.todoWorkEnd = 0, tmp.todoWorkBegin = 0;
    if(debug)
    {
        printf("queue init ok\n");
        fflush(stdout);
    }
}

void queueNodeDes(queue& tmp)
{
    pthread_cond_destroy(&tmp.queueFull);
    pthread_mutex_destroy(&tmp.queueMutex);
    free(tmp.todoWorkList);
    free(tmp.qboard);
}

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
char problem[problemNum][128];
bool problemStatus[problemNum];
int problemBegin, 
    problemEnd, 
    problemTotal, 
    problemSolved;
bool problemOver = false;


// 任务分配
int problemDistributeBegin;
bool distributeOver = false;


// 打印相关
int printBegin;
pthread_cond_t printOk;


// 线程pid
pthread_t getFNT, 
          readFT, 
          printRT, 
          distributeT;
pthread_t* workerT;

pthread_mutex_t problemMutex;
pthread_cond_t problemFull, 
               problemEmpty;

pthread_mutex_t workerMutex;
pthread_mutex_t fileOverM, problemOverM, distributeOverM;


// 工作线程
queue* threadQueue;


// 配置数据
int cores, 
    threadNum;

void* getFileName(void* args)
{
    if(debug)
    {
        printf("in getFileNameT\n");
        fflush(stdout);
    }
    // TODO:改善读入方法
    while(scanf("%s", fileName) != EOF)
    {
        if(debug)
        {
            printf("in getFileNameT while\n");
            fflush(stdout);
        }
        pthread_mutex_lock(&fileMutex);
        while ((fileEnd + 1) % files == fileBegin)
        {
            if(debug)
            {
                printf("getFileNameT wait\n");
                fflush(stdout);
            }
            pthread_cond_wait(&fileEmpty, &fileMutex);
        }
        strncpy(fileNameBuffer[fileEnd], fileName, fileChars);
        if(debug)
        {
            printf("getFNT:\treceive file[%d]:%s\n", fileEnd, fileNameBuffer[fileEnd]);
            fflush(stdout);
        }
        fileEnd = (fileEnd + 1) % files;

        pthread_cond_broadcast(&fileFull);
        pthread_mutex_unlock(&fileMutex);
    }
    pthread_mutex_lock(&fileOverM);
    fileOver = true;
    if(debug)
    {
        printf("getFileName over!\n");
        fflush(stdout);
    }
    pthread_mutex_unlock(&fileOverM);
    return NULL;
}

void* readFile(void* args)
{
    if(debug)
    {
        printf("in readFile Thread!\n");
        fflush(stdout);
    }
    while (1)
    {
        if(debug)
        {
            printf("in readFile Thread while!\n");
            fflush(stdout);
        }
        pthread_mutex_lock(&fileMutex);
        pthread_mutex_lock(&fileOverM);
        if(fileOver && fileBegin == fileEnd)
        {
            pthread_mutex_unlock(&fileMutex);
            pthread_mutex_unlock(&fileOverM);
            pthread_mutex_lock(&problemOverM);
            problemOver = true;
            pthread_mutex_unlock(&problemOverM);
            if(debug)
            {
                printf("readFile Over!\n");
                fflush(stdout);
            }
            break;
        }
        else
        {
            pthread_mutex_unlock(&fileOverM);
        }
        while(fileBegin == fileEnd)
        {
            if(debug)
            {
                printf("readFileT waiting!\n");
                printf("fileOver = %d\n", fileOver);
                fflush(stdout);
            }
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
            while((problemEnd + 1) % problemNum == printBegin)   // 问题满
            {
                if(debug)
                {
                    printf("PB full\n");
                    fflush(stdout);
                }
                pthread_cond_wait(&problemEmpty, &problemMutex);
            }
            if(read(fd, problem[problemEnd], 82) > 0 
                && problem[problemEnd][0] != '\n')
            {
                if(debug)
                {
                    printf("readFT:\treceive problem %d\n", problemEnd);
                    fflush(stdout);
                    for(int i = 0; i < int(strlen(problem[problemEnd])); i ++)
                        printf("%c", problem[problemEnd][i]);
                    printf("\n");
                    fflush(stdout);
                }
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

        fileBegin = (fileBegin + 1) % files;

        pthread_cond_broadcast(&fileEmpty);
        pthread_mutex_unlock(&fileMutex);
    }
    return NULL;
}

void* distribute(void* args)
{
    if(debug)
    {
        printf("in distributeT\n");
        fflush(stdout);
    }
    int ii = 0;
    while(1)
    {
        if(debug)
        {
            // printf("in distributeT while\n");
            fflush(stdout);
        }
        pthread_mutex_lock(&problemMutex);
        while(problemEnd == problemDistributeBegin)
        {
            pthread_mutex_lock(&problemOverM);
            if(problemOver)
            {
                if(debug)
                {
                    printf("distribute over!\n");
                    fflush(stdout);
                }
                pthread_mutex_unlock(&problemMutex);
                pthread_mutex_unlock(&problemOverM);
                pthread_mutex_lock(&distributeOverM);
                distributeOver = true;
                pthread_mutex_unlock(&distributeOverM);
                // 离开前最后以此唤醒所有线程
                for(int i = 0; i < threadNum; i ++)
                {
                    pthread_cond_signal(&threadQueue[i].queueFull);
                }
                return NULL;
            }
            else
            {
                pthread_mutex_unlock(&problemOverM);
            }
            if(debug)
            {
                printf("distributT waiting!\n");
                fflush(stdout);
            }
            pthread_cond_wait(&problemFull, &problemMutex);
            if(debug)
            {
                printf("wake up when problemBegin = %d & problemEnd = %d\n", problemBegin, problemEnd);
                fflush(stdout);
            }
        }
        // 考虑释放问题的锁？

        pthread_mutex_lock(&threadQueue[ii].queueMutex);
        queue& tmp = threadQueue[ii];
        
        assert(tmp.todoWorkEnd >= 0 && tmp.todoWorkEnd < workPerThread && "out todoWorkList\n");
        assert(problemDistributeBegin >= 0 && problemDistributeBegin < problemNum && "out pro list\n");
        
        if(tmp.todoWorkBegin == (tmp.todoWorkEnd + 1) % workPerThread)
        {
            pthread_mutex_unlock(&tmp.queueMutex);
            pthread_mutex_unlock(&problemMutex);
            ii = (ii + 1) % threadNum;
            if(debug)
            {
                printf("worker[%d] todoworkList full\n", ii);
                fflush(stdout);
            }
            usleep(100);
            continue;
        }

        if(debug)
        {
            printf("Assign work[%d] to worker[%d]\n", problemDistributeBegin, ii);
        }

        tmp.todoWorkList[tmp.todoWorkEnd] = problemDistributeBegin;
        tmp.todoWorkEnd = (tmp.todoWorkEnd + 1) % workPerThread;
        problemDistributeBegin = (problemDistributeBegin + 1) % problemNum;
        ii = (ii + 1) % threadNum;
        pthread_cond_signal(&tmp.queueFull);
        pthread_mutex_unlock(&tmp.queueMutex);

        pthread_cond_signal(&problemEmpty);
        pthread_mutex_unlock(&problemMutex);
    }
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
    if(debug)
    {
        printf("in worker thread[%d]\n", id);
        fflush(stdout);
    }
    while(1)
    {
        if(debug)
        {
            printf("in worker thread[%d] while\n", id);
            fflush(stdout);
        }
        queue& tmp = threadQueue[id];
        pthread_mutex_lock(&tmp.queueMutex);
        while (tmp.todoWorkBegin == tmp.todoWorkEnd)
        {
            pthread_mutex_lock(&distributeOverM);
            if(distributeOver)
            {
                if(debug)
                {
                    printf("worker[%d] over\n", id);
                    fflush(stdout);
                }
                pthread_mutex_unlock(&tmp.queueMutex);
                pthread_mutex_unlock(&distributeOverM);
                return NULL;
            }
            else
            {
                pthread_mutex_unlock(&distributeOverM);
            }

            if(debug)
            {
                printf("in worker thread[%d] wait\n", id);
                fflush(stdout);
            }
            pthread_cond_wait(&tmp.queueFull, &tmp.queueMutex);
        }
        int pp = tmp.todoWorkList[tmp.todoWorkBegin];
        if(debug)
        {
            printf("woker[%d]:get problem %d\n", id, pp);
            fflush(stdout);
        }
        solve_sudoku_dancing_links(problem[pp], tmp.qboard, pp);
        tmp.todoWorkBegin = (tmp.todoWorkBegin + 1) % workPerThread;
        problemStatus[pp] = true;
        pthread_mutex_unlock(&tmp.queueMutex);
        pthread_cond_signal(&printOk);

        pthread_mutex_lock(&workerMutex);
        problemSolved ++;
        if(debug)
        {
            printf("worker[%d] solve problem[%d]\n", id, pp);
        }

        pthread_mutex_unlock(&workerMutex);
    }
    return NULL;
}

void* print(void*)
{
    if(debug)
    {
        printf("in print thread!\n");
        fflush(stdout);
    }
    while(1)
    {
        pthread_mutex_lock(&problemMutex);
        while(printBegin == problemEnd)     // 已打印最后一个
        {
            pthread_mutex_lock(&distributeOverM);
            if(distributeOver)
            {
                if(debug)
                {
                    printf("print T over!\n");
                    fflush(stdout);
                }
                pthread_mutex_unlock(&distributeOverM);
                pthread_mutex_unlock(&problemMutex);
                return NULL;
            }
            else
            {
                pthread_mutex_unlock(&distributeOverM);
            }
            if(debug)
            {
                // TODO:输出优化
                printf("print wait!\n");
                fflush(stdout);
            }
            pthread_cond_wait(&printOk, &problemMutex);
        }
        pthread_mutex_unlock(&problemMutex);

        if(problemStatus[printBegin])
        {
            for(int i = 0; i < 81; i ++)
            {
                printf("%c", problem[printBegin][i]);
            }
            printf("\n");
            fflush(stdout);
            problemStatus[printBegin] = false;
            printBegin = (printBegin + 1) % problemNum;
        }
    }
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

    pthread_mutex_init(&workerMutex, NULL);

    pthread_mutex_init(&fileOverM, NULL);
    pthread_mutex_init(&problemOverM, NULL);
    pthread_mutex_init(&distributeOverM, NULL);

    pthread_cond_init(&printOk, NULL);

    threadQueue = (queue*)malloc(threadNum * sizeof(queue));
    assert(threadQueue != NULL);
    for(int i = 0; i < threadNum; i ++)
    {
        queueNodeInit(threadQueue[i]);
    }

    if(debug)
    {
        printf("init function OK\n");
        fflush(stdout);
    }
}

int main()
{
    cores = get_nprocs();
    if(debug)
    {
        printf("Available cores in the machine : %d\n", cores);
        fflush(stdout);
    }
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

    if(pthread_create(&distributeT, NULL, distribute, NULL) != 0)
    {
        printf("fail to create distributeT\n");
        fflush(stdout);
        exit(1);
    }

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

    pthread_create(&printRT, NULL, print, NULL);

    pthread_join(getFNT, NULL);
    pthread_join(readFT, NULL);
    pthread_join(distributeT, NULL);
    for(int i = 0; i < threadNum; i ++)
    {
        pthread_join(threadQueue[i].pid, NULL);
    }

    pthread_join(printRT, NULL);

    if(debug)
    {
        printf("Free begin!\n");
    }
    for(int i = 0; i < threadNum; i ++)
    {
        queueNodeDes(threadQueue[i]);
    }

    if(debug)
    {
        printf("main over!\n");
    }

    return 0;
}
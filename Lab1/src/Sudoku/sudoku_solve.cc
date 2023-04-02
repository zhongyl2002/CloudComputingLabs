#include<stdio.h>
#include<iostream>
#include<sys/sysinfo.h>
#include<pthread.h>
#include<unistd.h>
#include<fcntl.h>
#include<cstring>
#include<cstdlib>

#include"sudoku.h"

#define debug true
#define BATCH_SIZE 512 * 4;

using namespace std;

const int problemNum = 10000,
          workPerThread = 50,
          files = 8,
          fileChars = 100;

char fileNameBuffer[files][fileChars];
char fileName[fileChars];

// 问题数据暂存
char problem[problemNum][128], problemStatus[problemNum];
pthread_t getFNT, readFT, printRT, testT, distributeT;
pthread_t* workerT;
// char* workerBuf[cores][workPerThread];

pthread_mutex_t fileMutex;
pthread_cond_t fileFull, fileEmpty;

pthread_mutex_t problemMutex;
pthread_cond_t problemFull, problemEmpty;

pthread_mutex_t workerMutex;
pthread_cond_t workerFull, workerEmpty;

int cores, threadNum;
int fileBegin, fileNum;
int problemInBuffer, problemBegin, problemTotal, problemSolved;
int* todoWorkNum, *todoWorkListBegin, *todoWorkListEnd;
int** todoWorkList;
int freeSlots, maxSlots, problemDistributeBegin;

void* getFileName(void* args)
{
    while(scanf("%s", fileName) != EOF)
    {
        pthread_mutex_lock(&fileMutex);
        while (fileNum == files)
        {
            pthread_cond_wait(&fileEmpty, &fileMutex);
        }
        strncpy(fileNameBuffer[(fileBegin + fileNum) % files], fileName, fileChars - 1);
        if(debug)printf("getFNT:\treceive file:%s\n", fileNameBuffer[(fileBegin + fileNum) % files]);
        fileNum ++;

        pthread_cond_broadcast(&fileFull);
        pthread_mutex_unlock(&fileMutex);
    }
    return NULL;
}

void* readFile(void* args)
{
    if(debug)printf("in readFile Thread!\n");
    while (1)
    {
        if(debug)printf("in readFile Thread while!\n");
        pthread_mutex_lock(&fileMutex);
        while(fileNum == 0)
        {
            if(debug)printf("readFileT waiting!\n");
            pthread_cond_wait(&fileFull, &fileMutex);
        }

        // 打开并读取文件
        int fd = open(fileNameBuffer[fileBegin], O_RDONLY);
        if(fd == -1)
        {
            printf("file open failed!\n");
            exit(1);
        }

        pthread_mutex_lock(&problemMutex);
        while(problemInBuffer < problemNum
                 && read(fd, problem[(problemBegin  + problemInBuffer) % problemNum], 82) > 0)
        {
            // 读文件每行末尾的'\n'
            // int tmp = read(fd, &endLine, 1);
            if(problem[problemBegin][0] != '\n')
            {
                while(problemInBuffer == problemNum)
                {
                    pthread_cond_wait(&problemEmpty, &problemMutex);
                }
                if(debug)printf("readFT:\treceive problem %d\n", problemBegin + problemInBuffer);
                if(debug)
                {
                    for(int i = 0; i < strlen(problem[(problemBegin  + problemInBuffer) % problemNum]); i ++)
                        printf("%c", problem[(problemBegin  + problemInBuffer) % problemNum][i]);
                    printf("\n");
                }                
                problemInBuffer ++;
                problemTotal ++;
                pthread_cond_broadcast(&problemFull);
            }
        }
        pthread_mutex_unlock(&problemMutex);

        fileBegin = (++fileBegin) % files;
        fileNum --;

        pthread_cond_broadcast(&fileEmpty);
        pthread_mutex_unlock(&fileMutex);
    }
    if(debug)printf("readFile over!\n");
    return NULL;
}

void* distribute(void* args)
{
    if(debug) printf("in distribute thread!\n");
    while (1)
    {
        pthread_mutex_lock(&problemMutex);
        while(problemInBuffer == 0)
        {
            pthread_cond_wait(&problemFull, &problemMutex);
        }

        pthread_mutex_lock(&workerMutex);
        int i = 0;
        for(; i < threadNum && (problemDistributeBegin - problemBegin < problemInBuffer); i ++)
        {
            if(todoWorkNum[i] < workPerThread)
            {
                while(freeSlots == 0)
                {
                    pthread_cond_wait(&workerEmpty, &workerMutex);
                }
                if(debug) printf("distribute thread : assign work[%d] to worker[%d]\n", problemDistributeBegin, i);
                todoWorkList[i][(todoWorkListBegin[i] + todoWorkNum[i]) % workPerThread] = problemDistributeBegin;
                todoWorkNum[i] ++;
                problemDistributeBegin = (problemDistributeBegin + 1) % problemNum;
                pthread_cond_broadcast(&workerFull);
            }
        }
        pthread_mutex_unlock(&workerMutex);
        
        pthread_cond_broadcast(&problemEmpty);
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
    if(debug) printf("in worker thread\n");
    init_neighbors();
    while(1)
    {
        pthread_mutex_lock(&workerMutex);
        while(todoWorkNum[id] == 0)
        {
            if(debug) printf("worker waiting!\n");
            pthread_cond_wait(&workerFull, &workerMutex);
        }
        pthread_cond_broadcast(&problemEmpty);
        pthread_mutex_unlock(&workerMutex);

        if(debug) printf("woker[%d]:get work %d\n", id, todoWorkListBegin[id]);
        input(problem[todoWorkListBegin[id]]);
        solve_sudoku_dancing_links(problem[todoWorkListBegin[id]]);
        
        pthread_mutex_lock(&workerMutex);
        problemStatus[todoWorkListBegin[id]] = '1';
        todoWorkNum[id] --;
        todoWorkListBegin[id] = (todoWorkListBegin[id] + 1) % workPerThread;
        pthread_mutex_unlock(&workerMutex);

        pthread_mutex_lock(&problemMutex);
        problemSolved ++;
        pthread_mutex_unlock(&problemMutex);
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

    pthread_cond_init(&workerFull, NULL);
    pthread_cond_init(&workerEmpty, NULL);
    pthread_mutex_init(&workerMutex, NULL);

    workerT = (pthread_t*)malloc(threadNum * sizeof(pthread_t));
    todoWorkNum = (int*)malloc(threadNum * sizeof(int));
    todoWorkListBegin = (int*)malloc(threadNum * sizeof(int));
    todoWorkNum = (int*)malloc(threadNum * sizeof(int));
    todoWorkList = (int**)malloc(threadNum * sizeof(int*));
    for(int i = 0; i < threadNum; i ++)
        todoWorkList[i] = (int*)malloc(workPerThread * sizeof(int));
    freeSlots = threadNum * workPerThread;
}

void waitForAllJobsDone()
{
    //Lazily wait all the worker threads finish their wget jobs
    while(1)
    {
        if(debug)printf("waitT:\tin waitT!\n");
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

    init();

    if(pthread_create(&getFNT, NULL, getFileName, NULL) != 0)
    {
        printf("fail to create get-file-name-thread!\n");
        exit(1);
    }

    if(pthread_create(&readFT, NULL, readFile, NULL) != 0)
    {
        printf("fail to create read-file-thread!\n");
        exit(1);
    }
/*
    if(pthread_create(&distributeT, NULL, distribute, NULL) != 0)
    {
        printf("fail to create distributeT\n");
        exit(1);
    }

    int threadId[threadNum];
    for(int i = 0; i < threadNum; i ++)
    {
        threadId[i] = i;
        if(pthread_create(&workerT[i], NULL, worker, &threadId[i]) != 0)
        {
            printf("fail to create workerT : %d\n", i);
            exit(1);
        }
    }
    
    for(int i = 0; i < threadNum; i ++)
        pthread_join(workerT[i], NULL);
*/
    waitForAllJobsDone();

    return 0;
}
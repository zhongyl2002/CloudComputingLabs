#include "kvstore2pcsystem.h"
#include <iostream>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <fstream>
#include <sstream>
using namespace std;
kvstore2pcsystem::kvstore2pcsystem(int argc, char* argv[])
{
    char *string_ = new char[2];
    fln = new char[255];
    mode = -1;
    static struct option long_options[]{
        {"config_path", required_argument, NULL, 1},
    };
    int opt;
    int option_index = 0;
    if((opt = getopt_long(argc, argv, string_, long_options, &option_index)) != -1){
        cout << "optarg = " << optarg << endl;
        strcpy(fln, optarg);
    }else {
        cout << "input error! please input correct config_path" << endl;
    }
    init();
}
void kvstore2pcsystem::init()
{
    struct stat s;
    // stat(fln ,&s)构造stat结构体变量，用于存储文件的状态信息，stat函数的返回值为0表示获取状态信息成功，非零值表示失败
    // S_IFREG是一个宏定义，用于表示普通文件的类型
    if (stat(fln ,&s)==0 && s.st_mode & S_IFREG) {
        fstream input(fln);
        string str;
        while (!input.eof())
        {
            getline(input, str);
            int i = 0, n = str.length();
            while(i < n && str[i] ==' ') i++;
            if(str[i] == '!') continue;
            string parameterName, value;
            stringstream ss(str);
            // ss.rdbuf()获取缓冲区指针
            // in_avail()为stringstream类的成员函数，指示缓冲区是否有数据可读
            if(ss.rdbuf() -> in_avail() != 0){
                ss>>parameterName;
                if (parameterName == "mode") {
                    if(ss.rdbuf() -> in_avail() == 0) {
                        perror("mode is null in the configuration file!!!");
                    }
                    ss >> value;
                    if (value == "coordinator") {
                        mode = 0;
                    }
                    else if(value == "participant") {
                        mode = 1;
                    }else {
                        perror("mode is error in the configuration file！！！");
                    }
                }
                else if(parameterName == "coordinator_info" or parameterName == "participant_info")
                    continue;
                else {
                    perror("Configuration file format error");
                }
            }
        }
    }else {
        cout << "input error! please input correct config_path !!!" << endl;
    }
}
kvstore2pcsystem::~kvstore2pcsystem()
{
}


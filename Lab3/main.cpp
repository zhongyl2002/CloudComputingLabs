#include "kvstore2pcsystem.h"
#include "coordinator.h"
#include "participant.h"
#include <iostream>
using namespace std;
int main(int argc, char * argv[])
{
    kvstore2pcsystem KVS2PC(argc, argv);
    cout << KVS2PC.getfln() << endl; 
    cout << "mode = " << KVS2PC.getmode() << endl;
    if(KVS2PC.getmode() == 0) {
        coordinator myCoordinator(KVS2PC.getfln());
        myCoordinator.start();
    }
    else if(KVS2PC.getmode() == 1) {
        participant myParticipant(KVS2PC.getfln());
        myParticipant.start();
    }
    else {
        perror("mode error!!!");
        exit(0);
    }
}
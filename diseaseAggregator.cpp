//Maria Anna Kanellaki - 1115201400060

#include <iostream>
#include <cerrno>
#include <unistd.h>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <assert.h>

#include "Date.h"
#include "namesList.h"
#include "Functions.h"
using namespace std;


string fifo = "fifo_to_worker";
string receive_fifo = "fifo_to_aggr";
int signal_id;


int main(int argc, char* argv[]) {
    //checks arguments
    if (argc != 7 || strcmp(argv[1], "-w") != 0 || strcmp(argv[3], "-b") != 0 || strcmp(argv[5], "-i") != 0)
    {
        cout <<"Input: wrong format: should be ./diseaseAggregator –w numWorkers -b bufferSize -i input_dir"<<endl;
        exit(-1);
    }

    int bufferSize, numWorkers;
    assert(numWorkers = atoi(argv[2]));
    assert(bufferSize = atoi(argv[4]));
    const char *input_dir = argv[6];

    //signal handler struct
    struct sigaction sig_act;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = SA_SIGINFO | SA_RESTART;
    sig_act.sa_sigaction = handle_signal;

    int i, j;
    pid_t cpids[numWorkers];                                          //array that stores ids of children that are created
    int fd_to[numWorkers];                                            //array that stores fds of fifos
    int fd_from[numWorkers];                                          //array that stores fds of receiver fifos
    bool ready[numWorkers];                                           //array that stores if a worker is ready or not
    namesList * allCountries[numWorkers];                             //array that stores a list of countries for each worker
    namesList * dir_names = new namesList;                            //list that stores all subdir names

    for (i=0; i<numWorkers; i++)
    {
        allCountries[i] = new namesList();
        ready[i] = true;
    }

    //counts subdirectories and creates a list with their names
    int count=0;                                                      //counts subdirectories
    DIR *d;
    d = opendir(input_dir);
    if (!d)
    {
        perror("open: input dir");
        exit(1);
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != nullptr)
    {
        if (strcmp(dir->d_name, "..")==0 || strcmp(dir->d_name, ".")==0)
            continue;
        nameNode * dirNode = new nameNode(dir->d_name);
        dir_names->push(dirNode);
        count++;
    }

    //creates two fifos for each worker, one to send and one to receive
    for (i=0; i<numWorkers; i++)
    {
        if (mkfifo((fifo+to_string(i)).c_str() , 0666) ==  -1)
        {
            if ( errno != EEXIST ) {
                perror("receiver: mkfifo");
                exit (2);
            }
        }
        if (mkfifo((receive_fifo+to_string(i)).c_str() , 0666) ==  -1)
        {
            if ( errno != EEXIST ) {
                perror("receiver: mkfifo aggr");
                exit (10);
            }
        }
    }

    //calculates files per worker considering the case where some workers take less than others
    int dirs_per_folder = count / numWorkers;
    int modular = count % numWorkers;
    if (modular)
        dirs_per_folder++;

    pid_t pid, cpid;
    bool flag=false;
    for (i=0; i<numWorkers; i++)
    {
        //creates numWorkers processes from parent which execute "Worker" executable
        pid = fork();
        cpids[i] = pid;                                               //stores child id
        if(pid == -1)
        {
            perror("Fork error");
            exit(6);
        }
        if (pid != 0)   //parent writes to each child's fifo and continues to make workers
        {
            //opens each fifo for write
            fd_to[i] = open((fifo+to_string(i)).c_str(), O_WRONLY);
            if (fd_to[i] == -1)
            {
                perror("opening fifo");
                exit(3);
            }
            if (!flag && modular &&i >= modular)    //when modulars are covered the subdirs per worker are reduced
            {                                       //happens only once
                dirs_per_folder--;
                flag=true;
            }

            int size;
            for (j=0; j<dirs_per_folder; j++)       //writes dirs_per_folder dir names in fifo
            {
                //for one dir writes: sizeof dir name and dir name
                write_string_to_fifo(fd_to[i], bufferSize, dir_names->getHead()->getName());    //writes 1st element
                allCountries[i]->push(new nameNode(dir_names->getHead()->getName()));           //adds it to worker's position in array
                dir_names->pop(dir_names->getHead()->getName());                                //and pops it
            }
            write_string_to_fifo(fd_to[i], bufferSize, "ready!");                               //writes ready when done
            ready[i] = false;                                                                   //worker busy with reading these
        }
        else            //child writes dirs_per_folder subdir names in fifo and execs worker main
            execl("./worker", "./worker","-b",argv[4],"-i",argv[6],"-fr",(fifo+to_string(i)).c_str(),"-fs",(receive_fifo+to_string(i)).c_str(), NULL);
    }

    //reads summary statistics from each worker and prints them
    for (i=0; i<numWorkers; i++)
    {
        //opens receiver fifo for read
        fd_from[i] = open((receive_fifo+to_string(i)).c_str(), O_RDONLY);
        if (fd_from[i] < 0)
        {
            perror("open fifo receiver");
            exit(10);
        }

        read_and_print_fifo(fd_from[i], bufferSize);
        ready[i] = true;                                //worker free
    }
    //program continues after all workers are ready
    //prints menu of options
    cout <<endl<< "Options:"<<endl;
    cout << "1. /listCountries"<<endl;
    cout << "2. /diseaseFrequency disease date1 date2 (optional: country)"<<endl;
    cout << "3. /topk-AgeRanges k country disease date1 date2"<<endl;
    cout << "4. /searchPatientRecord recordID"<<endl;
    cout << "5. /numPatientAdmissions disease date1 date2 (optional: country)"<<endl;
    cout << "6. /numPatientDischarges disease date1 date2 (optional: country)"<<endl;
    cout << "7. /exit"<<endl;

    if (sigaction(SIGINT, &sig_act, &sig_act) < 0)
    {
        perror("sigaction error");
        exit(-2);
    }
    string input;
    char *str;
    char delimit[]=" \t\n";

    do{
        cout <<endl<<"Enter your input:" << endl<< endl;
        getline(cin, input);
        char * line = (char*)malloc(input.length());
        strcpy(line, input.c_str());

        if (input.find("/listCountries") != string::npos)               //lists all countries with their worker's id
        {
            for (i=0; i<numWorkers; i++)
            {
                nameNode *temp = allCountries[i]->getHead();
                while (temp)
                {
                    cout <<temp->getName()<<" "<<cpids[i]<< endl;
                    temp = temp->getNext();
                }
                delete temp;
            }
        }
        else if (input.find("/diseaseFrequency") != string::npos)                  //prints count of diseased people for given disease and given dates
        {                                                                             //and maybe country
            //passes command to all workers sums their results and prints the sum
            count=0;
            for (i=0; i<numWorkers; i++)
            {
                write_string_to_fifo(fd_to[i], bufferSize, input);                    //writes input string in all fifos
                write_string_to_fifo(fd_to[i], bufferSize, "ready!");

                count += read_int_fifo(fd_from[i], bufferSize);                       //reads counts sum of results from all workers
            }
            cout << count << endl;                                                    //prints total count
        }
        else if (input.find("/topk-AgeRanges") != string::npos)                    //prints age stats for given disease, country and dates
        {
            //finds worker that has this country and passes him the command through the fifo
            strtok(line, delimit);                                                    //splits command name
            if (!strtok(NULL, delimit)) //k
            {
                cout << "Error. Needs number to execute" << endl;
                continue;
            }
            if (!(str = strtok(NULL, delimit))) //country
            {
                cout << "Error. Needs country to execute" << endl;
                continue;
            }
            for (i=0; i<numWorkers; i++)
            {
                if (allCountries[i]->SearchName(str))                             //finds where country belongs
                    break;
            }
            if (i == numWorkers)
            {
                cout << "Error. Country not found"<< endl;
                continue;
            }
            write_string_to_fifo(fd_to[i], bufferSize, input);                    //writes input string in fifo
            write_string_to_fifo(fd_to[i], bufferSize, "ready!");

            read_and_print_fifo(fd_from[i], bufferSize);                          //reads and prints each line read from the worker
        }
        else if (input.find("/searchPatientRecord") != string::npos)
        {
            //passes command to all workers and prints the result
            for (i=0; i<numWorkers; i++)
            {
                write_string_to_fifo(fd_to[i], bufferSize, input);                    //writes input string in all fifos
                write_string_to_fifo(fd_to[i], bufferSize, "ready!");

                read_and_print_fifo(fd_from[i], bufferSize);                          //reads counts sum of results from all workers
            }
        }
        else if (input.find("/numPatientAdmissions") != string::npos || input.find("/numPatientDischarges") != string::npos)              //prints count of people that are still admitted
        {
            strtok(line, delimit);                                                     //splits command name
            if (!strtok(NULL, delimit)) {
                cout << "Error. Needs disease name to execute" << endl;
                continue;
            }
            if (!strtok(NULL, delimit)) {
                cout << "Error. Needs dates to execute" << endl;
                continue;
            }
            if (!strtok(NULL, delimit)) {
                cout << "Error. Needs two dates to execute" << endl;
                continue;
            }
            str = strtok(NULL, delimit);

            if (str)                //finds which worker has country and sends request there
            {
                for (i=0; i<numWorkers; i++)
                {
                    if (allCountries[i]->SearchName(str))
                        break;
                }
                if (i==numWorkers)
                {
                    cout << "Country doesn't exist"<< endl;
                    continue;
                }

                write_string_to_fifo(fd_to[i], bufferSize, input);                    //writes input string in all fifos
                write_string_to_fifo(fd_to[i], bufferSize, "ready!");

                read_and_print_fifo_pair(fd_from[i], bufferSize);                     //reads and prints country names with their sums
            }
            else
            {
                for (i=0; i<numWorkers; i++)                                          //sends request to all workers
                {
                    write_string_to_fifo(fd_to[i], bufferSize, input);                //writes input string in all fifos
                    write_string_to_fifo(fd_to[i], bufferSize, "ready!");

                    read_and_print_fifo_pair(fd_from[i], bufferSize);                 //reads and prints country names with their sums
                }
            }
        }
        else if (input == "/exit")
        {
            free(line);
            break;
        }
        else
            cout << "Error. Wrong input" << endl;
    }
    while (input != "/exit");

    //kills children and frees all memory
    int status;
    for (i=0; i<numWorkers; i++)
    {
        if(kill(cpids[i], SIGKILL) == -1)
        {
            perror("kill error");
            exit(20);
        }
        close(fd_to[i]);
        close(fd_from[i]);
        unlink((fifo+to_string(i)).c_str());
        unlink((receive_fifo+to_string(i)).c_str());

        delete allCountries[i];
        wait(&status);
    }
    delete dir_names;
    closedir(d);
    cout << endl<< "exiting"<<endl;
    exit(0);
}
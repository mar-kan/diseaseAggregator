//Kanellaki Maria Anna - 1115201400060


#include <iostream>
#include <cerrno>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>

#include "PatientList.h"
#include "HashTable.h"
#include "Functions.h"
#include "DateList.h"
#include "DiseaseList.h"

using namespace std;

#define entries 1000
#define bucketSize 20

int signal_id;

int main(int argc, char *argv[]) {
    //cout << "WorkerMain :" <<getpid()<< endl;

    //takes arguments -b bufferSize -i inputFiles -fr fifo_receiver -fs fifo_sender
    if (argc != 9 || strcmp(argv[1], "-b") != 0 || strcmp(argv[3], "-i") != 0 || strcmp(argv[5], "-fr") != 0 || strcmp(argv[7], "-fs") != 0)
    {
        cout << "Invalid arguments for Worker" << endl;
        exit(-1);
    }
    int bufferSize, fd_send;

    assert(bufferSize = atoi(argv[2]));
    string input_dir = argv[4];

    const char * fifo = argv[6];
    const char * fifo_send = argv[8];

    //signal handler struct
    struct sigaction sig_act;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = SA_SIGINFO | SA_RESTART;
    sig_act.sa_sigaction = handle_signal;

    //initializes worker's data structures
    PatientList * patients = new PatientList();                                              //creates empty list of patients
    HashTable * diseaseTable = new HashTable(entries, bucketSize);                           //create empty disease hash table
    HashTable * countryTable = new HashTable(entries, bucketSize);                           //create empty country hash table

    DateList * myDates = new DateList();                                                     //stores all dates of worker with their diseases and age stats

    int age_stats[4];                                                                        //stores num of cases for each age group for summary stats
    for (int i=0; i<4; i++)
        age_stats[i] = 0;

    //opens fifo for read
    int fd = open(fifo, O_RDONLY);
    if (fd < 0)
    {
        cout << "open fifo"<< endl;
        exit(1);
    }

    //reads all fifo until '0' is read as a size
    short int size;
    while (read(fd, &size, sizeof(short int)) == sizeof(short int))
    {
        if (size == 0)
        {
            cout << "ERROR"<< endl;
            break;
        }

        char dir_name[size + 1];
        if (size < bufferSize) {
            if (read(fd, dir_name, size) != size) {
                perror("read error (size)");
                exit(2);
            }
            dir_name[size] = '\0';
        }
        else    //builds the dir_name by bufferSize bytes per iteration
        {
            int bytes_read = 0, read_size;
            while (bytes_read < size)
            {
                if (size - bytes_read < bufferSize)
                    read_size = size - bytes_read;
                else
                    read_size = bufferSize;

                if (read(fd, dir_name + bytes_read, read_size) != read_size)
                {
                    perror("read error (bufferSize)");
                    exit(3);
                }
                bytes_read += read_size;
            }
            dir_name[bytes_read] = '\0';
        }
        if (strcmp(dir_name, "ready!")==0)
            break;

        chdir(input_dir.c_str());
        DIR *d;
        d = opendir(dir_name);
        if (!d)
        {
            perror("open subdir");
            exit(4);
        }
        chdir(dir_name);

        struct dirent *file;
        while ((file = readdir(d)) != NULL)                                                    //reads every file's name
        {
            if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0)
                continue;

            string date = file->d_name;
            FILE *fp = fopen(date.c_str(), "r");                                        //opens each file for read
            if (!fp)
            {
                cout << "Error in opening file " << date << " of " << dir_name << endl;
                perror("fopen file");
                exit(5);
            }

            size_t lsize = 0;
            char *line = NULL;
            PatientRecord *rec = NULL;
            while (getline(&line, &lsize, fp) != -1)                                           //reads file line by line
            {
                //creates new patient record
                char *dat = new char[date.length()];
                strcpy(dat, date.c_str());

                rec = new PatientRecord(line, dir_name, dat);                                 //creates PatientRecord from line, dir and file names
                if (!rec->isValid())                                                          //if record is not valid deletes it and continues to next line
                {
                    cout << "ERROR" << endl;
                    delete rec;
                    continue;
                }
                if (rec->getStatus() == "EXIT")     //checks if patient had entered
                {
                    PatientRecord*dtemp;
                    if ((dtemp = patients->SearchRecord(rec->getRecordId())))
                    {   //if id exists, updates its exit date
                        dtemp->setExitDate(rec->getExitDate());
                    }
                    else
                    {
                        cout << "ERROR"<< endl;
                        delete dtemp;
                    }
                    delete rec;                     //deletes new record and continues
                    continue;
                }

                //inserts it in all data structs
                patients->push(new PatientNode(rec));                                         //inserts node in list

                diseaseTable->InsertToTable(rec,rec->getDiseaseId());                         //inserts record in disease table
                countryTable->InsertToTable(rec,rec->getCountry());                           //inserts record in country table

                if (!myDates->SearchDate(rec->getEntryDate()->getAsString()))                 //inserts record in date list
                    myDates->push(new DateNode(rec));

                DiseaseList * thisDateDis = myDates->SearchDate(rec->getEntryDate()->getAsString())->getMyDiseases();
                DiseaseNode * dnode;                                   //inserts record in disease list of this country
                if ((dnode = thisDateDis->SearchDisease(rec->getDiseaseId())))                //if it exists calculates age stats
                {
                    dnode->AddAgeToStats(rec->getAge());                                      //calculate summary stats
                    continue;
                }
                else
                {
                    delete dnode;
                    thisDateDis->push(new DiseaseNode(rec));
                }
            }
            fclose(fp);
            free(line);
        }
        closedir(d);
        chdir("../..");
    }

    //send statistics
    //opens sender fifo for write
    fd_send = open(fifo_send, O_WRONLY);
    if (fd_send < 0)
    {
        perror("open fifo sender");
        exit(6);
    }
    DateNode * dtemp = myDates->getHead();                                //for each date
    while (dtemp)
    {
        write_string_to_fifo(fd_send, bufferSize, dtemp->getDate()->getAsString());  //writes date name
        write_string_to_fifo(fd_send, bufferSize, dtemp->getRecord()->getCountry()); //writes country name

        DiseaseNode * temp = dtemp->getMyDiseases()->getHead();
        while (temp)                                                      //for each disease
        {
            write_string_to_fifo(fd_send, bufferSize, temp->getDisease());//writes disease name

            //cases for each age range
            int age_group=0;
            for (int i=0; i<4; i++)
            {
                int cases = temp->getAgeStats(i);
                string str;                                               //creates string for each age range
                if (i==0)
                    str = "Age range "+to_string(age_group)+"-"+to_string(age_group+20)+" years: "+ to_string(cases)+" cases";
                else if (i==3)
                    str = "Age range "+to_string(age_group)+"+ years: "+ to_string(cases)+" cases";
                else
                    str = "Age range "+to_string(age_group+1)+"-"+to_string(age_group+20)+" years: "+ to_string(cases)+" cases";

                write_string_to_fifo(fd_send, bufferSize, str);           //writes string
                age_group += 20;
            }
            temp = temp->getNext();
        }
        dtemp = dtemp->getNext();
    }
    write_string_to_fifo(fd_send, bufferSize, "ready!");                  //writes ready when done

    //from this point on, worker is always ready to read
    int total=0, success=0;
    while (true)
    {
        if (sigaction(SIGINT, &sig_act, &sig_act) < 0)
        {
            perror("sigaction error");
            exit(-2);
        }
        //reads requests
        while (read(fd, &size, sizeof(short int)) == sizeof(short int))     //reads sizeof str
        {
            if (size == 0)
            {
                cout <<"ERROR"<<endl;
                break;
            }

            char str[size + 1];
            if (size < bufferSize)
            {
                if (read(fd, str, size) != size)                            //reads str
                {
                    perror("read error (size)");
                    exit(2);
                }
                str[size] = '\0';
            }
            else    //builds the str by bufferSize bytes per iteration
            {
                int bytes_read = 0, read_size;
                while (bytes_read < size)
                {
                    if (size - bytes_read < bufferSize)
                        read_size = size - bytes_read;
                    else
                        read_size = bufferSize;

                    if (read(fd, str + bytes_read, read_size) != read_size)
                    {
                        perror("read error (bufferSize)");
                        exit(3);
                    }
                    bytes_read += read_size;
                }
                str[bytes_read] = '\0';
            }
            if (strcmp(str, "ready!")==0)
                break;

            char *date1, *date2, *country, *disease;
            char delimit[]=" \t\n";
            string input = str;

            //executes command and sends results back from the fifo
            if (input.find("/diseaseFrequency") != string::npos)                       //prints count of diseased people for given disease and given dates
            {                                                                             //and maybe country
                total++;
                strtok(str, delimit);                                                     //splits command name
                if (!(disease = strtok(NULL, delimit))) {
                    cout << "Error. Needs disease name to execute" << endl;
                    continue;
                }
                if (!(date1 = strtok(NULL, delimit))) {
                    cout << "Error. Needs dates to execute" << endl;
                    continue;
                }
                if (!(date2 = strtok(NULL, delimit))) {
                    cout << "Error. Needs two dates to execute" << endl;
                    continue;
                }
                country = strtok(NULL, delimit);

                Date *d1 = new Date(date1);
                Date *d2 = new Date(date2);
                if (!d1->isValid() || !d2->isValid())
                {
                    delete d1;
                    delete d2;
                    continue;
                }
                int count;
                if (country)                                                              //finds worker with country and writes its pipe
                    count = countryTable->CountAllCountry(country, disease, d1, d2);
                else                                                                      //no country writes to all workers
                    count = diseaseTable->CountAll(disease, d1, d2);

                write_string_to_fifo(fd_send, bufferSize, to_string(count));
                write_string_to_fifo(fd_send, bufferSize, "ready!");
                success++;
            }
            else if (input.find("/topk-AgeRanges") != string::npos)
            {
                total++;
                strtok(str, delimit);                                                    //splits command name
                int k;

                if (!(country = strtok(NULL, delimit)))
                {
                    cout << "Error. Needs number to execute" << endl;
                    continue;
                }
                else
                    assert(k = atoi(country));

                if (!(country = strtok(NULL, delimit)))
                {
                    cout << "Error. Needs country to execute" << endl;
                    continue;
                }
                if (!(disease = strtok(NULL, delimit)))
                {
                    cout << "Error. Needs disease to execute" << endl;
                    continue;
                }
                if (!(date1 = strtok(NULL, delimit)))
                {
                    cout << "Error. Needs dates to execute" << endl;
                    continue;
                }
                if (!(date2 = strtok(NULL, delimit)))                              //if there is only one date then the argument is not valid
                {
                    cout << "Error. Needs 2 dates to execute"<< endl;
                    continue;
                }
                Date * d1 = new Date(date1);
                Date * d2 = new Date(date2);

                if (!d1->isValid() || !d2->isValid())
                {
                    delete d1;
                    delete d2;
                    continue;
                }

                int age=0;
                int age_counts[2][4];                  //array that stores the counts for each age group
                for (int i=0; i<4; i++)
                {
                    age_counts[0][i]=age;
                    age_counts[1][i]=0;
                    age += 20;
                }

                //calculates ages from patient list
                PatientNode *temp = patients->getHead();
                int total_count=0;
                while (temp)
                {
                    if (temp->getRecord()->getCountry() == country && temp->getRecord()->getDiseaseId() == disease)
                    {
                        if (d1->earlierThan(temp->getRecord()->getEntryDate()) && temp->getRecord()->getEntryDate()->earlierThan(d2) ||
                        temp->getRecord()->getExitDate() && d1->earlierThan(temp->getRecord()->getExitDate()) && temp->getRecord()->getExitDate()->earlierThan(d2))
                        {
                            if (temp->getRecord()->getAge() <= 20)
                                age_counts[1][0]++;
                            else if (temp->getRecord()->getAge() <= 40)
                                age_counts[1][1]++;
                            else if (temp->getRecord()->getAge() <= 60)
                                age_counts[1][2]++;
                            else
                                age_counts[1][3]++;

                            total_count++;
                        }
                    }
                        temp = temp->getNext();
                }

                //sort array (bubble)
                for (int i=0; i<4; i++)
                {
                    for (int j=0; j<4-i; j++)
                    {
                        if (age_counts[1][j+1] > age_counts[1][j])
                        {
                            int tmp = age_counts[0][j+1];
                            age_counts[0][j+1] = age_counts[0][j];
                            age_counts[0][j] = tmp;

                            tmp = age_counts[1][j+1];
                            age_counts[1][j+1] = age_counts[1][j];
                            age_counts[1][j] = tmp;
                        }
                    }
                }
                //write top k in one string
                string all = "";
                for (int i=0; i<k; i++)
                {
                    if (i == 4)
                        break;

                    if (age_counts[0][i] == 0)
                        all.append("0-20: "+to_string(age_counts[1][i]*100/total_count)+"% \n");
                    else if (age_counts[0][i] == 20)
                        all.append("21-40: "+to_string(age_counts[1][i]*100/total_count)+"% \n");
                    else if (age_counts[0][i] == 40)
                        all.append("41-60: "+to_string(age_counts[1][i]*100/total_count)+"% \n");
                    else
                        all.append("60+: "+to_string(age_counts[1][i]*100/total_count)+"%\n");
                }

                //write string to fifo
                write_string_to_fifo(fd_send, bufferSize, all);
                write_string_to_fifo(fd_send, bufferSize, "ready!");
                success++;
            }
            else if (input.find("/searchPatientRecord") != string::npos)
            {
                total++;
                strtok(str, delimit);                                                    //splits command name

                if (!(country = strtok(NULL, delimit)))
                {
                    cout << "Error. Needs recordID to execute" << endl;
                    continue;
                }
                PatientRecord *rec = patients->SearchRecord(country);
                if (!rec)
                {
                    write_string_to_fifo(fd_send, bufferSize, "no_result");
                    write_string_to_fifo(fd_send, bufferSize, "ready!");
                    continue;
                }
                write_string_to_fifo(fd_send, bufferSize, rec->getWhole());
                write_string_to_fifo(fd_send, bufferSize, "ready!");
                success++;
            }
            else if (input.find("/numPatientAdmissions") != string::npos || input.find("/numPatientDischarges") != string::npos)
            {
                total++;
                strtok(str, delimit);                                                     //splits command name
                if (!(disease = strtok(NULL, delimit))) {
                    cout << "Error. Needs disease name to execute" << endl;
                    continue;
                }
                if (!(date1 = strtok(NULL, delimit))) {
                    cout << "Error. Needs dates to execute" << endl;
                    continue;
                }
                if (!(date2 = strtok(NULL, delimit))) {
                    cout << "Error. Needs two dates to execute" << endl;
                    continue;
                }
                country = strtok(NULL, delimit);

                Date *d1 = new Date(date1);
                Date *d2 = new Date(date2);
                if (!d1->isValid() || !d2->isValid())
                {
                    delete d1;
                    delete d2;
                    continue;
                }

                string status;
                if (input.find("/numPatientAdmissions") != string::npos)
                    status = "ENTER";
                else
                    status = "EXIT";

                int count;
                if (country)                //finds country and calculates its count
                {
                    count = countryTable->CountEnterOrExitCountry(country, disease, d1, d2, status);

                    //writes country name and count
                    write_string_to_fifo(fd_send, bufferSize, country);
                    write_string_to_fifo(fd_send, bufferSize, to_string(count));
                    write_string_to_fifo(fd_send, bufferSize, "ready!");
                }
                else                                    //writes and counts same as above but for every country of worker
                {                                       //through the country hash table
                    Bucket ** table = countryTable->getTable();
                    for (int i=0; i<countryTable->getSize(); i++)
                    {
                        count=0;
                        if (table[i])
                        {
                            table[i]->getTree()->countEnterOrExitCountry(table[i]->getTree()->getRoot(), d1, d2, disease, &count, status);

                            write_string_to_fifo(fd_send, bufferSize, table[i]->getFirst()->getKey());
                            write_string_to_fifo(fd_send, bufferSize, to_string(count));
                            write_string_to_fifo(fd_send, bufferSize, "ready!");
                        }
                    }
                }
                success++;
            }
            else
            {   total++;
                cout << "ERROR" << endl;
                write_string_to_fifo(fd_send, bufferSize, "no_result");
                write_string_to_fifo(fd_send, bufferSize, "ready!");
            }
        }
    }

    //deletes every structure
    delete diseaseTable;
    delete countryTable;
    delete patients;
    delete myDates;

    close(fd);
    exit(0);
}
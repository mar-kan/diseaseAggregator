# diseaseAggregator
project in c/cpp for System Programming course in universiry for processes 


# Script create_inflies.h
execution instructions: $ ./create_infiles.sh diseasesFile.txt countriesFile.txt <nameOfDir> <numOfFiles> <numOfRecs>
  
  ------------------
  
The script is used to create the input files required for the execution of the project.
  
  
# Main project
  
  compilation instructions: $ make
  execution instructions: $ ./diseaseAggregator –w <numWorkers> -b <bufferSize> -i <input_dir>
  
  ------------------
  
  This project is an extesion of the diseaseMonitor project (https://github.com/mar-kan/diseaseMonitor) with about the same functionality but processes were added to this version, using forks. 
  
 2 executionables were created, diseaseAggregatorMain, which starts the program, and WorkerMain, which is called through the child processes
    of the diseaseAggregator.
  
  When the program starts, all input files are divided between the workers (processes) that are created. When the process is over and the data structures full, the user may input a command from the menu outputed and the aggregator sends the command to a worker. The worker searches its records and sends the results back to the aggregator that finally prints it.
  
  ------------------------
  
  # Communication
  
  The communication between the aggregator and the workers is succeeded by messages in the following format:
    
        2 bytes for size of its name
        its name
        ready! when the writing is done to show the change of the state of the workers.
        
        When a worker hasn't founnd any results for a query, it writes "no_result".

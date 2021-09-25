all: diseaseAggregator worker

diseaseAggregator: diseaseAggregator.o namesList.o Functions.o Date.o
	g++ -g3 diseaseAggregator.o namesList.o Functions.o Date.o -o diseaseAggregator

worker: Worker.o Date.o PatientList.o PatientRecord.o HashTable.o Tree.o DateList.o DiseaseList.o Functions.o
	g++ -g3 Worker.o Date.o PatientList.o PatientRecord.o HashTable.o Tree.o DateList.o DiseaseList.o Functions.o -o worker

diseaseAggregator.o: diseaseAggregator.cpp
	g++ -std=c++11 -g3 -c diseaseAggregator.cpp

Worker.o: Worker.cpp
	g++ -std=c++11 -g3 -c Worker.cpp

Date.o: Date.cpp Date.h
	g++ -std=c++11 -g3 -c Date.cpp

PatientList.o: PatientList.cpp PatientList.h
	g++ -g3 -std=c++11 -c PatientList.cpp

PatientRecord.o: PatientRecord.cpp PatientRecord.h
	g++ -g3 -std=c++11 -c PatientRecord.cpp

HashTable.o: HashTable.cpp HashTable.h
	g++ -std=c++11 -g3 -c HashTable.cpp

Tree.o: Tree.cpp Tree.h
	g++ -g3 -std=c++11 -c Tree.cpp

namesList.o: namesList.cpp namesList.h
	g++ -g3 -std=c++11 -c namesList.cpp

CountryList.o: DateList.cpp DateList.h
	g++ -g3 -std=c++11 -c DateList.cpp

DiseaseList.o: DiseaseList.cpp DiseaseList.h
	g++ -g3 -std=c++11 -c DiseaseList.cpp

Functions.o: Functions.cpp Functions.h
	g++ -g3 -std=c++11 -c Functions.cpp

clean:
	rm *.o diseaseAggregator worker

diseasesFile=$1
countriesFile=$2
input_dir=$3

#typechecks numbers
if [ $4 -lt 0 ]
then
  echo "Invalid number of directories"
  exit
else
  numFilesPerDirectory=$4
fi

if [ $5 -lt 0 ]
then
    echo "Invalid number of files"
    exit
else
    numRecordsPerFile=$5
fi

#creates array with countries from countries file
numOfCountries=0
set countries
while read line; do
  countries[numOfCountries]=$line
  numOfCountries=$((numOfCountries+1))
done < $countriesFile

#creates array with diseases from diseases file
numOfDiseases=0
set diseases
while read line;
do
  diseases[numOfDiseases]=$line
  numOfDiseases=$((numOfDiseases+1))
done < $diseasesFile

#creates and enters new directory
mkdir "$input_dir"
cd "$input_dir" || exit

#creates subdirectories for all the countries
i=0
while [ $i -lt $numOfCountries ]
do
  dirName=${countries[i]}
  mkdir "$dirName"

  #in every country subdir, creates numFilesPerDirectory files with random date names
  cd "$dirName" || exit
  j=0
  while [ $j -lt "$numFilesPerDirectory" ]
  do
    day=$(($RANDOM % 30 + 1))
    month=$(($RANDOM % 12 + 1))
    year=$((RANDOM % 101 + 1920)) #year between 1920 and 2020

    fileName=$day'-'$month'-'$year
    touch $fileName

    #write records in new files
    k=0
    while [ $k -lt $numRecordsPerFile ]
    do
      #create rand data
      id=$RANDOM
      rand=$(($RANDOM % 2))
      if ([ $rand == 0 ])
      then
        status='EXIT'
      else
        status='ENTER'
      fi
      name=$(cat /dev/urandom | tr -dc 'a-zA-Z' | fold -w 13 | head -n 1)
      lastname=$(cat /dev/urandom | tr -dc 'a-zA-Z' | fold -w 13 | head -n 1)
      disease=${diseases[$RANDOM % $numOfDiseases]}
      age=$(($RANDOM % 121))

      #print it in file
      echo "$id $status $name $lastname $disease $age" >> $fileName

      k=$(( $k + 1 ))
    done
	  j=$(( $j + 1 ))
  done
  cd ..
  i=$(( $i + 1 ))
done

echo "Directories and files are ready"
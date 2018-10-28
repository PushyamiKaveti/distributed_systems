#!/bin/bash

set -e
echo "bringing down the docker containers brought up during previous docker-compose"
sudo docker-compose down
echo "\n Creating lab2 setup using docker compose up..."
sudo docker-compose up &
sleep 10
sudo docker ps | grep -i "machine1"|awk '{print $1}' |grep -v CONT > hostlist
sudo docker ps | grep -i "machine2"|awk '{print $1}' |grep -v CONT >> hostlist
sudo docker ps | grep -i "machine3"|awk '{print $1}' |grep -v CONT >> hostlist
sudo cat hostlist
for i in `cat hostlist`; 
do 
sudo docker exec -it $i bash -c "hostname"; 
done

#for i in `cat hostlist`; 
#do 
#sudo docker exec -it $i bash -c "cd build ; ./prj2_mp -h ../hostnames.txt -p 10000 " ;
#sleep 2 
#done


#!/bin/bash


if [ -e $1 ]
then
	if [ "$(ls -A $1)" ]
	then 
		#if directory exists 
		active=() #array for active boards
		inactive=() #array for inactive boards
		#for every subdirectory
		for dir in $1/*
		do
			#check if it is a valid board directory (contains all necessary files)
			if [ -e "$dir/StC" ] && [ -e "$dir/StB" ] && [ -e "$dir/BtS" ] && [ -e "$dir/CtS" ] && [ -e "$dir/spid" ];
			then
				#if valid,check if server proccess is online
				pid=$(cat $dir/spid)
				if  ps -p $pid > /dev/null
				then
				        active+=($dir)
					echo BOARD $dir is ACTIVE		
				else
				        inactive+=($dir)
					echo BOARD $dir is NOT ACTIVE
				fi
			else
				echo $dir is not a valid board directory,or something is missing
			fi
		done

		echo RESULTS:
		#print active boards
		echo "${#active[@]}" BOARDS ACTIVE : 
		for a_board in "${active[@]}"
		do
		        echo "$a_board"
		done
	       
		#print inactive boards
		echo "${#inactive[@]}" BOARDS INACTIVE : 
		for i_board in "${inactive[@]}"
		do
		        echo "$i_board"
		done
	else
		echo "$1 is empty.No boards exist."
	fi     
else
	echo $1 does not exits
fi

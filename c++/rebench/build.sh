#!/bin/bash
# generate all binaries

START_DIR=`pwd`

function compile_microbenches {
  cd epcc

  for coreNum in *
  do
	if [[ "$coreNum" =~ ^[0-9]+$ ]]
	then
		echo Build EPCC benchmark for cores=$coreNum
		cd $coreNum
		DIR=`pwd`
		cd ../../../
		rm *.epcc *.dynamic *.epcc-simple *.absoh *.lioh
		NUM_PARTICIPANTS=$coreNum make 
		mv *.epcc *.epcc-simple *.absoh *.lioh $DIR/
		mv *.dynamic $DIR/../../dynamic/$coreNum/
		rm *.dynamic
		cd -
		cd ..	   
	fi
  done
}

function complie_splash {
  cd $START_DIR
  cd splash

  SPLASH_BENCHS="lu barnes cholesky fft ocean_cp radiosity radix water-spatial"

  for coreNum in *
  do
	if [[ "$coreNum" =~ ^[0-9]+$ ]]
	then
		echo Build SLASH-2 benchmarks for cores=$coreNum
		cd $START_DIR/splash/$coreNum
		DIR=`pwd`
		cd $START_DIR/../bench
		for bench in $SPLASH_BENCHS
		do
			cd $bench
			make clean
			echo Make in `pwd`
			NUM_PARTICIPANTS=$coreNum make
			if [[ "Darwin" = `uname` ]]
			then 
				mv *.[A-Z]* $DIR
			else
				find . -maxdepth 1 -name "*.[A-Z]*" -executable -exec mv {} $DIR \;
			fi
			cd ..
		done
	fi
  done
}

compile_microbenches
complie_splash



## Contents

+  [About The Project](#About-The-Project)
+ [Introduction](#Introduction)
+ [Getting Started](#Getting-Started)
+ [Dependencies](#Dependencies)
+ [Running the Debugger](#Running-the-Debugger)	
    + [Debugging with function name](#Debugging-with-function-name)
    + [Debugging by entering the lines](#Debugging-by-entering-the-lines)
	+ [Output](#Output)
+ [References](#References)


## About The Project

### Project: Software Fault Injector with advanced Operating Systems capabilities
#### Problem Definition
Hardware failures can introduce errors due to several kinds of faults that can affect the software
during its execution. While it is quite easy to study the effect of the fault at hardware level, it is far
more complicated to evaluate the impact on a full application.

#### Objectives 
For this project the goal is building a fault injector able to inject specific faults into a software
image (at runtime) and track the effect of the fault on the execution, protecting at the same time
the OS, in order to collect the final effect without affecting the whole system. The injection will be
in the form of mutation of single instructions or data elements of the program, while the
classification of the error will be used to feed a Bayesian Model.



## Introduction

### SOFI (Software Fault Injector)

#### Debugger Features

Our debugger will support the following features:
+ Launch, halt, and continue execution
+ Set breakpoints on
+ Memory addresses
+ Source code lines
+ Function entry
+ Read and write registers and memory
+ Single stepping
+ Instruction
+ Step in
+ Step out
+ Step over
+ Print current source location
+ Print backtrace
+ Print values of simple variables
+ Multi-threaded debugging support

#### Types of Errors 
The runtime program will be injected in the Opcode or Data.

Injection in Opcode causes crash or hung mode (infinite loop) and injection in data causes SDC (Silent Data Corruption).

## Getting Started

Clone the repository and install all required modules

```
>> git clone https://github.com/tonyYSaliba/Software-Fault-Injector.git
```
Go to the directory 

```
>> cd Software-Fault-Injector
```
For creating make file 
```
>> make 
```
Then type this to build the project

```
>> cmake ../. 
```
Run the debugger 
```
>> ./sofi 
``` 


### Prerequisites 

#### Working Environment 
This software is carried out in C++. You can run it in Linux or Windows with C++ Compiler or Windows Subsystem for Linux (WSL)

#### IDE
Any IDE that compile c++ codes


## Dependencies 
+ Linenoise: For handling our command line input
+ libelfin:  For parsing the debug information

## Running the Debugger  


### Debugging with function name 
+ Enter the name of the program that you want to debug, you enter `unwinding.cpp`

+ Then enter how you want to inject: 
 
	1 for injecting through L1 and L2 lines
 
	2 for entering the name of function

+ you should type the name of function `tester`

+ Then Enter the injection type, you have 3 options: **opcode, data, register**

+ Enter the number of error injections to be performed

+ After the certain timeout the output will be printed 


### Debugging by entering the lines  
+ Enter the name of the program that you want to debug, you enter `stack_unwinding.cpp`

+ Then choose two lines for injecting, enter the line number for Line1 and Line2

+ You should enter the injection type (**opcode, data, register**)

+ Enter the number of injections


### Output 

You will get the responces of all the tests, this is the output: 


| Option | Description |
| ------ | ----------- |
| tid   | Thread Id, to show the number of threads that were executing |
| halt | 0 or 1, It is 1 if there is any halt, if we choose opcode as the injection type then program will enter to the hung mode |
| duration    | Duration time for thread execution, 0 for less than 1 second |
| sdc    | 0 or 1 if there is silent data corruption |
| code, error, singno, no    | To show if the program is crashed or not, no field explains what is happend inside the program|
| code:0, error:0, singno:0, no: Unknown signal    | if all the fields are 0 and no:Unknown signal, means the program executed successfuly |
| code:1, error:1, singno: with different numbers, no: fault explanation    | program is not executed successfuly|



## References


- https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1

- https://t-a-w.blogspot.com/2007/03/how-to-code-debuggers.html

- http://system.joekain.com/debugger/
  

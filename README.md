## Contents
-[About The Project](#About-The-Project)

-[Introduction](#Introduction)

-[Getting Started](#Getting-Started)

-[Running the Debugger](#Running-the-Debugger)

-[References](#References)



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

### Sofi: Software Fault Injector

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
The runtime program will be injected in the Opcode or Data. Injection in Opcode causes crash or hung mode (infinite loop) and injection in data causes SDC (Silent Data Corruption) 

## Getting Started

1- clone the repository and install all required modules

```
>> git clone https://github.com/tonyYSaliba/Software-Fault-Injector.git
```
2- go to the directory 

```
>> cd Software-Fault-Injector
```
3- for creating the make file type this 
```
>> make 
```
4- then type this to build the project

```
>> cmake ../. 
```
5- for running the debugger 
```
>> ./sofi 
``` 


### Prerequisites 

#### Working Environment 
This software is carried out in C++. You can run it in Linux or Windows with C++ Compiler or Windows Subsystem for Linux (WSL)

#### IDE
Any IDE that compile c++ codes


## Running the Debugger  

### Debugging with function name 
1- Enter the name of the program that you want to debug:
 you enter `unwinding`
 
2- Then enter how you want to inject: Enter 1 or 2 
 
1 for injecting through L1 and L2 lines 

2 for entering the name of function

3- if you choose 2: 
Enter the name of function `tester`

1- Then Enter the injection type, you have 3 options: **opcode, date, register**

2- Enter the number of error injections to be performed


### Debugging by entering the lines  
1- Enter the name of the program that you want to debug:
 you enter `stack_unwinding`

2- Enter Line1 and Line2 

3- Enter the injection type and number of injections


### Output 

You will get the responces of all the tests, this is the output: 

`**tid**` Thread Id (the number of threads that were executing)

`**halt**` 0 or 1 if there was any halt, if we choose opcode as the injection type then program will enter to the hung mode

`**sdc**` 0 or 1 if there was silent data corruption 

`**code, error, singno**` if the program was crashed or not 

`**code:0, error:0, singno:0, no: Unknown signal**` program executed successfuly 

`**code:1, error:1, singno: with different numbers, no: fault explanation**` program executed successfuly  


## References

- A. Vallero et al., "SyRA: Early System Reliability Analysis for Cross-Layer Soft Errors Resilience
in Memory Arrays of Microprocessor Systems," in IEEE Transactions on Computers, vol. 68, no.
5, pp. 765-783, 1 May 2019. 

- https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1

- https://t-a-w.blogspot.com/2007/03/how-to-code-debuggers.html

- http://system.joekain.com/debugger/
  

## Contents

+  [About The Project](#About-The-Project)
+ [Introduction](#Introduction)
+ [Prerequisites](#Prerequisites) 
+ [Getting Started](#Getting-Started)
+ [Dependencies](#Dependencies)
+ [Running the Debugger](#Running-the-Debugger)	
    + [Debugging with function name](#Debugging-with-function-name)
    + [Debugging by entering the lines](#Debugging-by-entering-the-lines)
	+ [Output](#Output)
+ [Screenshot of output](#Screenshot-of-output)
+ [References](#References)


## About The Project

### Software Fault Injector with advanced Operating Systems capabilities
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
+ Find local variables and arguments
+ Multi-threaded and Multi-process debugging support

#### Types of Errors 
The runtime program will be injected through these types of errors:
+ Opcode: causes crash or hang mode (infinite loop)
+ Data: causes SDC (Silent Data Corruption)
+ Register: causes SDC, crash or hang mode


## Prerequisites 

#### Working Environment 
This software is carried out in C++. you can run it in Linux or Windows with C++ Compiler or Windows Subsystem for Linux (WSL)


## Getting Started

Clone the repository

```
>> git clone https://github.com/tonyYSaliba/Software-Fault-Injector.git
```
Go to the directory 

```
>> cd Software-Fault-Injector
```
Create a directory for your build

```
>> mkdir dist
>> cd dist
```

Then type this to create the make file

```
>> cmake ../. 
```

then type this to build the project
```
>> make 
```

Run the debugger 
```
>> ./sofi 
``` 



## Dependencies 
+ libelfin:  For parsing the debug information

## Running the Debugger  


### Debugging with function name 
+ Enter the name of the program that you want to debug. (e.g: `unwinding` , `hello` or `variable`)

+ Then enter how you want to inject: 
 
	`1` for injecting through L1 and L2 lines (you should also specify the name of the file. e.g: `stack_unwinding.cpp`)
 
	`2` for entering the name of function (you should specify the name of the function in this format: `function`)

+ Then Enter the injection type, you have 3 options: **Opcode, Data, Register**

+ Enter the number of error injections to be performed

### Debugging by entering the lines  
+ Enter the name of the program that you want to debug, you enter `stack_unwinding.cpp`

+ Then choose two lines for injecting, enter line number for Line1 and Line2

+ You should enter the injection type (**Opcode, Data, Register**)

+ Enter the number of injections


### Output 

You will get the response of all the tests, these are the output columns: 


| Option | Description |
| ------ | ----------- |
| tid   | Thread Id, to show the number of threads that were executing |
| halt | 0 or 1, It is 1 if there is any halt |
| duration    | Duration time for thread execution (in seconds), 0 for less than 1 second |
| sdc    | 0 or 1 if there is silent data corruption |
| code, error, singno, no    | To show if the program has crashed or not. The `no` field explains what has happened inside the program|
| code:0, error:0, singno:0, no: Unknown signal    | if all the fields are `0` and `no:Unknown signal`, means the program executed successfuly |
| code:1, error:1, singno: with different numbers, no: fault explanation    | program has not executed successfuly|

## Screenshot of output 

The sample output for Opcode injection

![Screenshot](https://i.ibb.co/2SGbF1C/output.jpg)

### Chart 
The frequency of errors for Opcode injection

![Screenshot](https://i.ibb.co/552sBf8/Chart.png")

## References


- https://eli.thegreenplace.net/2011/01/23/how-debuggers-work-part-1

- https://t-a-w.blogspot.com/2007/03/how-to-code-debuggers.html

- http://system.joekain.com/debugger/
  

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
_____

#### Types of Errors 
The runtime program will be injected in the Opcode or Data. Injection in Opcode causes crash or hung mode (infinite loop) and injection in Data ccauses SDC(Silent Data Corruption) 

#### Libraries to Install
Linenoise: For handeling command line input 
Libelfin: For parsing the debug information
 
#### Step-by-step running the debuger
1- Lunch the child process 
2- Set breakpoints 
3- ...

####


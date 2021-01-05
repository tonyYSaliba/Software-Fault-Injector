#include <vector>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/personality.h>
#include <unistd.h>
#include <sstream>
#include <iostream>

#include "linenoise.h"

#include "debugger.hpp"

using namespace minidbg;

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> out{};
    std::stringstream ss {s};
    std::string item;

    while (std::getline(ss,item,delimiter)) {
        out.push_back(item);
    }

    return out;
}

bool is_prefix(const std::string& s, const std::string& of) {
    if (s.size() > of.size()) return false;
    return std::equal(s.begin(), s.end(), of.begin());
}

void debugger::handle_command(const std::string& line) {
    auto args = split(line,' ');
    auto command = args[0];

    if (is_prefix(command, "cont")) {
        continue_execution();
    }
    else if(is_prefix(command, "break")) {
        std::string addr {args[1], 2}; //naively assume that the user has written 0xADDRESS
        set_breakpoint_at_address(std::stol(addr, 0, 16));
    }
    else if(is_prefix(command, "Y1")) {
        unsigned int i=0;
        while(true){
            ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
            int wait_status;
            auto options = 0;
            if(!waitpid(m_pid, &wait_status, options)){
                std::cout << "End of program\n";
                std::cout << i << std::endl;
                break;
            }
            
        }
    }
    else if(is_prefix(command, "Y2")) {
        ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);

        int wait_status;
        auto options = 0;
         if(waitpid(m_pid, &wait_status, options) == m_pid){
             std::cout << "waitpid() success"<< std::endl;
         }
         if(WIFEXITED(wait_status)){
            std::cout << "Exited"<< std::endl;
         }
         else if(WIFSIGNALED(wait_status)){
            std::cout << "Signaled"<< std::endl;
         }
         else if(WIFSTOPPED(wait_status)){
            std::cout << "Stopped"<< std::endl;
         }
         else if(WIFCONTINUED(wait_status)){
            std::cout << "Continued"<< std::endl;
         }
         
         std::cout << "status: "<<wait_status<< std::endl;

    }
    else {
        std::cerr << "Unknown command\n";
    }
}

void debugger::set_breakpoint_at_address(std::intptr_t addr) {
    std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
    breakpoint bp {m_pid, addr};
    bp.enable();
    m_breakpoints[addr] = bp;
}

void debugger::run() {
    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);

    char* line = nullptr;
    while((line = linenoise("minidbg> ")) != nullptr) {
        handle_command(line);
        linenoiseHistoryAdd(line);
        linenoiseFree(line);
    }
}

void debugger::continue_execution() {
    ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);

    int wait_status;
    auto options = 0;
    waitpid(m_pid, &wait_status, options);
}

void execute_debugee (const std::string& prog_name) {
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
        std::cerr << "Error in ptrace\n";
        return;
    }
    execl(prog_name.c_str(), prog_name.c_str(), nullptr);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Program name not specified";
        return -1;
    }

    auto prog = argv[1];

    auto pid = fork();
    if (pid == 0) {
        //child
        personality(ADDR_NO_RANDOMIZE);
        execute_debugee(prog);
    }
    else if (pid >= 1)  {
        //parent
        std::cout << "Started debugging process " << pid << '\n';
        debugger dbg{prog, pid};
        dbg.run();
    }
}


#include <vector>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/personality.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cstdlib>
#include <pthread.h>


#include <future>
#include <thread>
#include <chrono>

#include <mutex>              
#include <condition_variable> 

using namespace std::chrono_literals;
using namespace std::chrono; 

#include "linenoise.h"

#include "debugger.hpp"
#include "registers.hpp"

using namespace sofi;
using namespace std;

#define INFINITY 10 // Allowed duration of runtime (in seconds). After 10 seconds, SOFI considers that we entered hault mode.  
#define BUFFER_SIZE 4096 // Maximum amount of output.

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

class ptrace_expr_context : public dwarf::expr_context {
public:
    ptrace_expr_context (pid_t pid, uint64_t load_address) : 
       m_pid{pid}, m_load_address(load_address) {}

    dwarf::taddr reg (unsigned regnum) override {
        return get_register_value_from_dwarf_register(m_pid, regnum);
    }

    dwarf::taddr pc() {
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, m_pid, nullptr, &regs);
        return regs.rip - m_load_address;
    }

    dwarf::taddr deref_size (dwarf::taddr address, unsigned size) override {
        //TODO take into account size
        return ptrace(PTRACE_PEEKDATA, m_pid, address + m_load_address, nullptr);
    }

private:
    pid_t m_pid;
    uint64_t m_load_address;
};
template class std::initializer_list<dwarf::taddr>;
void debugger::read_variables(uint64_t* variables, int&size) {
    using namespace dwarf;

    auto func = get_function_from_pc(get_offset_pc());

    for (const auto& die : func) {
        if (die.tag == DW_TAG::variable || die.tag == DW_TAG::formal_parameter) {
            auto loc_val = die[DW_AT::location];

            //only supports exprlocs for now
            if (loc_val.get_type() == value::type::exprloc) {
                ptrace_expr_context context {m_pid, m_load_address};
                auto result = loc_val.as_exprloc().evaluate(&context);

                switch (result.location_type) {
                case expr_result::type::address:
                {
                    auto offset_addr = result.value;
                    auto value = read_memory(offset_addr);
                    variables[size] = offset_addr;
                    size++;
                    break;
                }

                case expr_result::type::reg:
                {
                    auto value = get_register_value_from_dwarf_register(m_pid, result.value);
                    variables[size] = result.value;
                    size ++;
                    break;
                }

                default:
                    throw std::runtime_error{"Unhandled variable location"};
                }
            }
            else {
                throw std::runtime_error{"Unhandled variable location"};
            }
        }
    }
}

symbol_type to_symbol_type(elf::stt sym) {
    switch (sym) {
    case elf::stt::notype: return symbol_type::notype;
    case elf::stt::object: return symbol_type::object;
    case elf::stt::func: return symbol_type::func;
    case elf::stt::section: return symbol_type::section;
    case elf::stt::file: return symbol_type::file;
    default: return symbol_type::notype;
    }
};

std::vector<symbol> debugger::lookup_symbol(const std::string& name) {
   std::vector<symbol> syms;

   for (auto& sec : m_elf.sections()) {
      if (sec.get_hdr().type != elf::sht::symtab && sec.get_hdr().type != elf::sht::dynsym)
         continue;

      for (auto sym : sec.as_symtab()) {
         if (sym.get_name() == name) {
            auto& d = sym.get_data();
            syms.push_back(symbol{ to_symbol_type(d.type()), sym.get_name(), d.value });
         }
      }
   }

   return syms;
}

void debugger::initialise_load_address() {
   //If this is a dynamic library (e.g. PIE)
   if (m_elf.get_hdr().type == elf::et::dyn) {
      //The load address is found in /proc/<pid>/maps
      std::ifstream map("/proc/" + std::to_string(m_pid) + "/maps");

      //Read the first address from the file
      std::string addr;
      std::getline(map, addr, '-');

      m_load_address = std::stol(addr, 0, 16);
   }
}

uint64_t debugger::offset_load_address(uint64_t addr) {
   return addr - m_load_address;
}

uint64_t debugger::offset_dwarf_address(uint64_t addr) {
   return addr + m_load_address;
}

void debugger::remove_breakpoint(std::intptr_t addr) {
    if (m_breakpoints.at(addr).is_enabled()) {
        m_breakpoints.at(addr).disable();
    }
    m_breakpoints.erase(addr);
}

void debugger::step_out() {
    auto frame_pointer = get_register_value(m_pid, reg::rbp);
    auto return_address = read_memory(frame_pointer+8);

    bool should_remove_breakpoint = false;
    if (!m_breakpoints.count(return_address)) {
        set_breakpoint_at_address(return_address);
        should_remove_breakpoint = true;
    }

    continue_execution();

    if (should_remove_breakpoint) {
        remove_breakpoint(return_address);
    }
}

void debugger::step_in() {
   auto line = get_line_entry_from_pc(get_offset_pc())->line;

   while (get_line_entry_from_pc(get_offset_pc())->line == line) {
      single_step_instruction_with_breakpoint_check();
   }

   auto line_entry = get_line_entry_from_pc(get_offset_pc());
   print_source(line_entry->file->path, line_entry->line);
}

void debugger::single_step_instruction() {
    ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
    wait_for_signal();
}

void debugger::single_step_instruction_with_breakpoint_check() {
    //first, check to see if we need to disable and enable a breakpoint
    if (m_breakpoints.count(get_pc())) {
        step_over_breakpoint();
    }
    else {
        single_step_instruction();
    }
}

uint64_t debugger::read_memory(uint64_t address) {
    return ptrace(PTRACE_PEEKDATA, m_pid, address, nullptr);
}

void debugger::write_memory(uint64_t address, uint64_t value) {
    ptrace(PTRACE_POKEDATA, m_pid, address, value);
}

uint64_t debugger::get_pc() { // get program counter
    return get_register_value(m_pid, reg::rip);
}

uint64_t debugger::get_offset_pc() {
   return offset_load_address(get_pc());
}

void debugger::set_pc(uint64_t pc) { // chnage program counter (used in handling breakpoints)
    set_register_value(m_pid, reg::rip, pc);
}

dwarf::die debugger::get_function_from_pc(uint64_t pc) { // get function using program counter
    for (auto &cu : m_dwarf.compilation_units()) {
        if (die_pc_range(cu.root()).contains(pc)) {
            for (const auto& die : cu.root()) {
                if (die.tag == dwarf::DW_TAG::subprogram) {
                    bool status;
                    try{
                        status = true;
                        at_low_pc(die);
                        at_high_pc(die);
                    }
                    catch(exception e){
                        status = false;
                    }
                    if(status){
                        if (at_low_pc(die)<=pc && pc<= at_high_pc(die)) {
                            return die;
                        }                    
                    }
                }
            }
        }
    }

    throw std::out_of_range{"Cannot find function"};
}

dwarf::die debugger::get_function_from_name(const std::string& name) { // find function using its name
    for (const auto& cu : m_dwarf.compilation_units()) {
        for (const auto& die : cu.root()) {
            if (die.has(dwarf::DW_AT::name) && at_name(die) == name) {
                return die;
            }
        }
    }

    throw std::out_of_range{"Cannot find function"};
}



dwarf::line_table::iterator debugger::get_line_entry_from_pc(uint64_t pc) { // gets line using program counter
    for (auto &cu : m_dwarf.compilation_units()) {
        if (die_pc_range(cu.root()).contains(pc)) {
            auto &lt = cu.get_line_table();
            auto it = lt.find_address(pc);
            if (it == lt.end()) {
                throw std::out_of_range{"Cannot find line entry"};
            }
            else {
                return it;
            }
        }
    }

    throw std::out_of_range{"Cannot find line entry"};
}

void debugger::print_source(const std::string& file_name, unsigned line, unsigned n_lines_context) { // prints the lines of ccode that are near the current program counter. (Used for testing purposes only)
    std::ifstream file {file_name};

    //Work out a window around the desired line
    auto start_line = line <= n_lines_context ? 1 : line - n_lines_context;
    auto end_line = line + n_lines_context + (line < n_lines_context ? n_lines_context - line : 0) + 1;

    char c{};
    auto current_line = 1u;
    //Skip lines up until start_line
    while (current_line != start_line && file.get(c)) {
        if (c == '\n') {
            ++current_line;
        }
    }

    //Output cursor if we're at the current line
    std::cout << (current_line==line ? "> " : "  ");

    //Write lines up until end_line
    while (current_line <= end_line && file.get(c)) {
        std::cout << c;
        if (c == '\n') {
            ++current_line;
            //Output cursor if we're at the current line
            std::cout << (current_line==line ? "> " : "  ");
        }
    }

    //Write newline and make sure that the stream is flushed properly
    std::cout << std::endl;
}

siginfo_t debugger::get_signal_info() {
    siginfo_t info;
    ptrace(PTRACE_GETSIGINFO, m_pid, nullptr, &info);
    return info;
}

void debugger::single_step() {
    ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
}

void debugger::step_over_breakpoint() {
    if (m_breakpoints.count(get_pc())) {
        auto& bp = m_breakpoints[get_pc()];
        if (bp.is_enabled()) {
            bp.disable();
        }
    }
}

siginfo_t debugger::wait_for_signal() {
    int wait_status;
    // auto options = ;
    waitpid(m_pid, &wait_status, WSTOPPED | WUNTRACED);//WUNTRACED
    // cout<<"########### "<<m_pid<<endl;
    auto siginfo = get_signal_info();
    switch (siginfo.si_signo) {
    case SIGTRAP:
        handle_sigtrap(siginfo);
        break;
    default:
        // cout<<"Got signal("<<m_pid<<"): "<<strsignal(siginfo.si_signo)<<"; "<<siginfo.si_code<<endl;
        break;
    }

    return siginfo;
}

void debugger::handle_sigtrap(siginfo_t info) {
    switch (info.si_code) {
        //one of these will be set if a breakpoint was hit
    case SI_KERNEL:
    case TRAP_BRKPT:
    {
        set_pc(get_pc()-1);
        // std::cout << "Hit breakpoint at address 0x" << std::hex << get_pc() << std::endl;
        auto offset_pc = offset_load_address(get_pc()); //rember to offset the pc for querying DWARF
        auto line_entry = get_line_entry_from_pc(offset_pc);
        // print_source(line_entry->file->path, line_entry->line);
        return;
    }
    //this will be set if the signal was sent by single stepping
    // case TRAP_TRACE:
    //     return;
    default:
    //     if(info.si_code == 0)
    //         cout<<"Success"<<endl;
    //     else
    //         std::cout << "Unknown SIGTRAP code " << info.si_code << std::endl;
        return;
    }
}

void debugger::continue_execution() {
    step_over_breakpoint();
    ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);
    wait_for_signal();
}

void debugger::continue_execution_single_step() {
    step_over_breakpoint();
    single_step();
    wait_for_signal();
}

void debugger::dump_registers() { // print out all registers (used for testing purposes)
    for (const auto& rd : g_register_descriptors) {
        std::cout << rd.name << " 0x"
                  << std::setfill('0') << std::setw(16) << std::hex << get_register_value(m_pid, rd.r) << std::endl;
    }
}

bool is_suffix(const std::string& s, const std::string& of) { // utility function
    if (s.size() > of.size()) return false;
    auto diff = of.size() - s.size();
    return std::equal(s.begin(), s.end(), of.begin() + diff);
}

void debugger::set_breakpoint_at_function(const std::string& name) { //sets breapont using function name
    for (const auto& cu : m_dwarf.compilation_units()) {
        for (const auto& die : cu.root()) {
            if (die.has(dwarf::DW_AT::name) && at_name(die) == name) {
                auto low_pc = at_low_pc(die);
                auto high_pc = at_high_pc(die);
                auto entry = get_line_entry_from_pc(low_pc);
                ++entry; //skip prologue
                set_breakpoint_at_address(offset_dwarf_address(entry->address));
                auto exit = get_line_entry_from_pc(high_pc);
                set_breakpoint_at_source_line(at_name(cu.root()), exit->line-1);
            }
        }
    }
}

void debugger::get_function_start_and_end_addresses(const std::string& name, std::intptr_t& start_addr, std::intptr_t& end_addr) { 
    for (const auto& cu : m_dwarf.compilation_units()) {
        for (const auto& die : cu.root()) {
            if (die.has(dwarf::DW_AT::name) && at_name(die) == name) {
                auto low_pc = at_low_pc(die);
                auto high_pc = at_high_pc(die);
                auto entry = get_line_entry_from_pc(low_pc);
                ++entry; //skip prologue
                start_addr = offset_dwarf_address(entry->address);
                auto exit = get_line_entry_from_pc(high_pc);

                std::string file =  at_name(cu.root());
                int line = exit->line-1;
                bool status = false;
                while(!status){
                    for (const auto& cu : m_dwarf.compilation_units()) {
                        if (is_suffix(file, at_name(cu.root()))) {
                            const auto& lt = cu.get_line_table();

                            for (const auto& entry : lt) {
                                if (entry.is_stmt && entry.line == line) {
                                    end_addr = offset_dwarf_address(entry.address);
                                    status = true;
                                }
                            }
                        }
                    }
                    line = line - 1;
                }
            }
        }
    }
}



void debugger::set_breakpoint_at_source_line(const std::string& file, unsigned line) { // sets breakpoint using line of code
    for (const auto& cu : m_dwarf.compilation_units()) {
        if (is_suffix(file, at_name(cu.root()))) {
            const auto& lt = cu.get_line_table();

            for (const auto& entry : lt) {
                if (entry.is_stmt && entry.line == line) {
                    set_breakpoint_at_address(offset_dwarf_address(entry.address));
                    return;
                }
            }
        }
    }
}

void debugger::get_alligned_address(std::intptr_t& addr) { // gets the useful address to set the mutation target
    for (const auto& cu : m_dwarf.compilation_units()) {
        
        const auto& lt = cu.get_line_table();

        for (const auto& entry : lt) {
            if (entry.is_stmt && offset_dwarf_address(entry.address)>=addr) {
                addr = offset_dwarf_address(entry.address);
                return;
            }
        }
        
    }
}

void debugger::get_address_at_source_line(const std::string& file, unsigned line, intptr_t& addr) { // gets address using the line of code
    for (const auto& cu : m_dwarf.compilation_units()) {
        if (is_suffix(file, at_name(cu.root()))) {
            const auto& lt = cu.get_line_table();

            for (const auto& entry : lt) {
                if (entry.is_stmt && entry.line == line) {
                    addr = offset_dwarf_address(entry.address);
                    return;
                }
            }
        }
    }
}

void debugger::set_breakpoint_at_address(std::intptr_t addr) { // sets breakpoint at a certain address
    // std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
    breakpoint bp {m_pid, addr};
    bp.enable();
    m_breakpoints[addr] = bp;
}

void debugger::run() { //used to initialize the debugger
    wait_for_signal();
    initialise_load_address();
}

void debugger::mutate_opcode(std::intptr_t addr) { //opcode is changed at a random address
    int randomOpcode = rand() % 0xFF;   
    write_memory(addr, (read_memory(addr) & ~0xFF)|randomOpcode);
}

void debugger::mutate_register(std::intptr_t addr) {
    /*
This function is runned at the start of the program.
First, we have a random address 'addr', we set a breakpoint on that address, and we continue our execution normally.
When the breakpoint is hit we pick a random regiter to mutate.
*/
    set_breakpoint_at_address(addr);
    continue_execution();
    int randomRegister = rand() % 27 ;
    int randomValue = rand();
    set_register_value(m_pid, get_register_from_name(g_register_descriptors[randomRegister].name), randomValue);
}

void debugger::mutate_data(std::intptr_t addr){ // mutates data
/*
This function is runned at the start of the program.
First, we have a random address 'addr', we set a breakpoint on that address, and we continue our execution normally.
When the breakpoint is hit, we get the available variables (locally defined variables and arguments), and pick a random one to mutate.
*/

    set_breakpoint_at_address(addr);
    continue_execution();
    int size = 0;
    uint64_t* variables = new uint64_t[100];
    read_variables(variables, size);
    int i = rand() % size;
    // write_memory(variables[i], (((~0x1)&(read_memory(variables[i]))) | ~((0x1)&(read_memory(variables[i]))) ));
    write_memory(variables[i], (read_memory(variables[i]) +(rand()%10 +1)));
}

void execute_debugee (const std::string& prog_name) { // used to start tracing the debugee
    if (ptrace(PTRACE_TRACEME, 0, 0, 0) < 0) {
        std::cerr << "Error in ptrace\n";
        return;
    }
    execl(prog_name.c_str(), prog_name.c_str(), nullptr);
}


struct thread_arguments {
    string prog = "";
    string functionName = "";
    string fileName = "";
    int inputType = 0; 
    int L1 = 0;
    int L2 = 0;
    string injectionType = "";
    int numberOfTests = 0;
    long tid;
    debugger* debuggers;
};

void go() {
  std::unique_lock<std::mutex> lck(mtx);
  ready = true;
  cv.notify_all();
}
int get_ttl(int duration, int number_of_tests){ // used to calculate INFINITY value. But, we will be using INFINITY as 10 seconds. If you want to change the value of INFINITY, just change the value inside 'define INFINITY' at the start of the code.
    return (duration+1)*number_of_tests;
}
void set_timeout(int seconds){ // used to emulate halt mode (it puts the thread into sleep)
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}
string convertToString(char* a, int size) { 
    int i; 
    string s = ""; 
    for (i = 0; i < size; i++) { 
        s = s + a[i]; 
    } 
    return s; 
} 
void thread_function(void *arguments) { // main function of a thread
    struct thread_arguments *args = (struct thread_arguments *)arguments;

    auto start = high_resolution_clock::now(); //used to calculate the runtime duration

    long tid; // thread ID
    tid = args->tid;

    int filedesOut[2]; // Used to get std::cout of the debuggee 
    int filedesErr[2]; // Used to get std:cerr of the debuggee

    if (pipe(filedesOut) == -1) { // creation of pipes for communication for cout
        perror("pipe");
        exit(1);
    }
    if (pipe(filedesErr) == -1) { // creation of pipes for communication for cerr
        perror("pipe");
        exit(1);
    }
    /* Set O_NONBLOCK flag for the read end (pfd[0]) of the pipe. */
    if (fcntl(filedesOut[0], F_SETFL, O_NONBLOCK) == -1) { // for cout
        fprintf(stderr, "Call to fcntl failed.\n");
        exit(1);
    }
     if (fcntl(filedesErr[0], F_SETFL, O_NONBLOCK) == -1) { // for cerr
        fprintf(stderr, "Call to fcntl failed.\n");
        exit(1);
    }

    auto pid = fork();
    if (pid == 0) { // child will become the debuggee
        //child
        while ((dup2(filedesOut[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
        while ((dup2(filedesErr[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
        close(filedesOut[0]);
        close(filedesErr[0]);
        personality(ADDR_NO_RANDOMIZE); // to remove address randomization
        execute_debugee(args->prog); // begin debuggee (execl)
    }
    else if (pid >= 1)  {
        //parent
        // std::cout << "Start process " << pid << " on thread "<<tid<<endl;
        debugger dbg{args->prog, pid};
        dbg.run(); // run debugger
        args->debuggers[tid] = dbg; // store thread's debugger object to the thread
        // debugger& dbg = (args->debuggers[tid]);

        intptr_t addr1;
        intptr_t addr2;
        intptr_t addr;

        if(args->inputType == 1){ // inject errors using source lines
            dbg.get_address_at_source_line(args->fileName, args->L1, addr1);
            dbg.get_address_at_source_line(args->fileName, args->L2, addr2);
            addr = addr1 + rand() % ((addr2 - addr1) +1);
            dbg.get_alligned_address(addr);
        }
        else if (args->inputType == 2){ // inject errors using function name
            dbg.get_function_start_and_end_addresses(args->functionName,addr1, addr2);
            addr = addr1 + rand() % ((addr2 - addr1) +1);
            dbg.get_alligned_address(addr);
        }

        if (args->injectionType == "Opcode"){ // Opcode error injection
            dbg.mutate_opcode(addr);
        }
        else if (args->injectionType == "Register"){ // mutation inside random registers error injection
            dbg.mutate_register(addr);
        }
        else if (args->injectionType == "Data"){// random data corruption error injection
            dbg.mutate_data(addr);        
        }
        else if (args->injectionType == "init"){
            // cout<<"golden code: "<<tid<<endl;
        }
        else if (args->injectionType == "test"){ // for testing purposes
            if(tid == 5){
                set_timeout(15);
            }
        }
        
        close(filedesOut[1]);
        close(filedesErr[1]);

        char bufferOut[BUFFER_SIZE]; // used to store cout
        char bufferErr[BUFFER_SIZE]; // used to store cerr

        dbg.step_over_breakpoint();
        ptrace(PTRACE_CONT, dbg.m_pid, nullptr, nullptr);
        dbg.result = dbg.wait_for_signal();

        if(args->debuggers[tid].halt_mode == 0 && dbg.result.si_code == 0 && dbg.result.si_signo == 0 && dbg.result.si_errno == 0){
            // cout<<tid<<" enter"<<endl;
            auto stop = high_resolution_clock::now(); 
            auto duration = duration_cast<seconds>(stop - start); 
            dbg.duration = duration.count();
            dbg.ttl = get_ttl(dbg.duration, args->numberOfTests);
            // cout<<tid<<" check1 "<<endl;
            ssize_t countOut = read(filedesOut[0], bufferOut, sizeof(bufferOut));
            ssize_t countErr = read(filedesErr[0], bufferErr, sizeof(bufferErr));
            dbg.originalOut = convertToString(bufferOut, countOut);
            dbg.originalErr = convertToString(bufferErr, countErr);
            // cout<<tid<<" check2 "<<endl;
            if(args->debuggers[0].originalOut != dbg.originalOut){
                dbg.sdc = 1;
            }
            else if(args->debuggers[0].originalErr != dbg.originalErr){
                dbg.sdc = 1;
            }
            // cout<<tid<<" quit"<<endl;

        }
        else if (args->debuggers[tid].halt_mode != 0){
            dbg.halt_mode = 1;
        }
        close(filedesOut[0]);
        close(filedesErr[0]);

        args->debuggers[tid] = dbg;
        // cout<<"Exit pid "<<pid<<" and thread "<<tid<<" duration "<<args->debuggers[tid].duration<<endl;
    }

}


void *thread_function_init(void *arguments) { // function used while initializing the threads 
    struct thread_arguments *args = (struct thread_arguments *)arguments;

    // The below lines of code are used to set timeout on the thread's execution in a graceful way (clean way).
    // if the running time becomes bigger than INFINITY, the debugee will stop on executing, and the debugger wil consider that we are in halt mode.
    if(args->tid){
        std::unique_lock<std::mutex> lck(mtx);
        while (!ready) cv.wait(lck);
    }

    // function used to execute the main fuction of the thread.
    std::future<void> future = std::async(std::launch::async, [arguments](){ 
        thread_function(arguments);
    }); 
 
    std::future_status status;
    bool timeout_done = false;
    do {
        if(args->debuggers[0].ttl<INFINITY){
            status = future.wait_for(std::chrono::seconds(args->debuggers[0].ttl));
        }
        else {
            status = future.wait_for(std::chrono::seconds(INFINITY));
        }
        if (status == std::future_status::deferred) {
            // std::cout << "deferred\n";
        } else if (status == std::future_status::timeout && !timeout_done) {
            timeout_done = true;
            args->debuggers[args->tid].halt_mode = 1;
            kill((args->debuggers[args->tid]).m_pid, SIGTRAP);
            // std::cout << "timeout thread "<<args->tid<<"...\n";
        } else if (status == std::future_status::ready) {
            // std::cout << "ready!\n";
        }
    } while (status != std::future_status::ready ); 
    go();
    pthread_exit(NULL);
}
void print_header(){ // SOFI header
    cout
    <<"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMMMdy     ydNMMMMMMMMMMMMMMMMMMd          hMMd  yNMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMN    yyy    dMMMMMMMMMMMMMMMMMh          hMMd  yNMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMd   mMMMMy   NMMMMMNmmmNMMMMMMh   NMMMMMMMMMNNNNMMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMm    dmMMNNNNMMMNh       ydMMMh   NMMMMMMMMMh   MMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMMmy     yhmMMMMd    hddy    NMh          MMMh   MMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMMMMNmhy     hMN   yMMMMMN   yMh   yyyyyyyMMMh   MMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMhyyyMMMMNh   dm   hMMMMMMy   Mh   NMMMMMMMMMy   MMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMh   hNMMMd   dMy   dNMMmh   hMh   NMMMMMMMMMy   MMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMMh          hMMMh          dMMh   NMMMMMMMMMy   MMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMMMNmhyyyyhdNMMMMMNdhyyyyhmMMMMmhhhMMMMMMMMMMdhhhMMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"<<endl
    <<"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"<<endl
    <<endl;
}
int main(int argc, char* argv[]) { // main function for program SOFI.
    print_header(); // prints the SOFI title.
    srand(time(0)); // to enable random fault injections.

    thread_arguments init_vars; // arguments used during thread initialization.

    do{
        cout    << "Please enter name of the program that you want to debug..." << endl;
        cin     >> init_vars.prog;
    }while(init_vars.prog == "");

    do{
        cout    << "Please enter how you want to inject..." << endl;
        cout    << "Between lines L1 and L2 ?[1] or inside a function f ?[2]";
        cin     >> init_vars.inputType;
    }while(init_vars.inputType == 0);

    
    if(init_vars.inputType == 1){
        do{
            cout    << "Please enter file name..." << endl;
            cin     >> init_vars.fileName;
            cout    << "Please enter Line 1..." << endl;
            cin     >> init_vars.L1;
            cout    << "Please enter Line 2..." << endl;
            cin     >> init_vars.L2;
        }while(init_vars.L1 == 0 || init_vars.L2 == 0 || init_vars.fileName == "");
    }
    else if(init_vars.inputType == 2){
        do{
            cout    << "Please enter name of the function that you want to debug..." << endl;
            cin     >> init_vars.functionName;
        }while(init_vars.functionName == "");
    }


    do{
        cout    << "Please enter Injection type..." << endl;
        cin     >> init_vars.injectionType;
    }while(init_vars.injectionType == "");

    do{
        cout    << "Please enter the number of error injections to be performed..." << endl;
        cin     >>  init_vars.numberOfTests;
    }while(init_vars.numberOfTests<0);

    int rc;
    int i;
    pthread_t* threads = new pthread_t[init_vars.numberOfTests + 1]; // creation of threads that will handle the fault injections.
    debugger* debuggers = new debugger[init_vars.numberOfTests + 1]; // To each thread, we have his own debugger object that will be handling the fault injection.
    pthread_attr_t attr;
    void *status;
    init_vars.debuggers = debuggers;
    thread_arguments* init_vars_arr = new thread_arguments[init_vars.numberOfTests + 1];

    // Initialize and set thread joinable
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    for( i = 0; i < init_vars.numberOfTests + 1; i++ ) { // Create the threads.
        init_vars_arr[i] = init_vars;
        if(i == 0){ // this iteration is made for the golden execution, to get the correct output data (for SDC ~ silent data corruption)
            init_vars_arr[i].injectionType = "init";
        }
        init_vars_arr[i].tid = i;
        rc = pthread_create(&threads[i], &attr, thread_function_init, (void *)&(init_vars_arr[i]));
        if (rc) {
            cout << "Error:unable to create thread," << rc << endl;
            exit(-1);
        }
    }

    // free attribute and wait for the other threads
    pthread_attr_destroy(&attr);
    for( i = 0; i < init_vars.numberOfTests + 1; i++ ) {
        rc = pthread_join(threads[i], &status);
        if (rc) {
            cout << "Error:unable to join," << rc << endl;
            exit(-1);
        }
        // cout << "Main: completed thread id :" << i ;
        // cout << "  exiting with status :" << status << endl;
    }
    cout<<"***********************************************************"<<endl; // Print results
    for(int i=0; i<init_vars.numberOfTests + 1; i++){
        cout<<"- tid: "<<i<<" - halt: "<<debuggers[i].halt_mode<<" - duration: "<<debuggers[i].duration<<" - sdc: "<<debuggers[i].sdc<<" - code: "<<debuggers[i].result.si_code<<" - errno: "<<debuggers[i].result.si_code<<" - singno: "<<debuggers[i].result.si_signo<<" - no: "<<strsignal(debuggers[i].result.si_signo)<<endl;
    }
    cout<<"***********************************************************"<<endl;

    cout << "Main: program exiting." << endl;
    pthread_exit(NULL);

    return 0;
}

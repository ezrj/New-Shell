#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <string>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fcntl.h>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    //create copies of stdin/stdout 
    int inp = dup(STDIN_FILENO);
    int outp = dup(STDOUT_FILENO);
    vector<int> pids;
    int status;
    //variable stori prvious working directory (needs to be declared outside of the loop)
    //filesystem::path lastdirectory = filesystem::current_path();
    //string lastdir = lastdirectory.c_str();

    filesystem::path currentdirectory = filesystem::current_path();
    string currentdir = currentdirectory.c_str();

    filesystem::path lastdirectory = filesystem::current_path();
    string holddirectory;

    for (;;) {
        //implement iteration over vector of bg pid
        //waitpid() - using flag -- what flag? rewatch video
        for (vector<pid_t>::iterator it = pids.begin(); it != pids.end(); ++it) { //should it be it++ or ++it?? change if bugs
            pid_t pid = *it;
            waitpid(pid, &status, WNOHANG);}
        


        //comments from video
        //implement date/time with todo
        //implement username with getlogin()
        //You must use getenv(“USER”) and not getlogin() when implementing your shell prompt.
        string user = getenv("USER");
        // need date/time, username, and absolute path to current dir
        auto now = chrono::system_clock::now();
        time_t now_c = chrono::system_clock::to_time_t(now);

        //does not pass tests
        //setenv("TZ", "America/Chicago", 1); // "America/Chicago" is the IANA timezone identifier for Central Time
        //tzset();
        int tz_offset = -5;
        now_c += tz_offset * 3600;

        // Format the time as "Mon DD HH:MM:SS YYYY"
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%b %d %H:%M:%S", localtime(&now_c));
        //string dir;
        filesystem::path dir = filesystem::current_path();
        string currentdir = dir.c_str();
        //directory goes where shell is

        cout << RED << buffer << " " << WHITE << user << ":" << BLUE << currentdir << "$" << NC << " ";
        
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        //change directory - comments from video
        //if (dir) (cd < dir>) is -, then go to previous working directory
        //variable stori prvious working directory (needs to be declared outside of the loop)
        if (tknr.commands[0]->args[0] == "cd") {
            //three scenarios, -, .., or into a directory
            if (tknr.commands[0]->args[1] == "-")  {
                filesystem::path holddir = filesystem::current_path();
                string holddirectory = holddir.c_str();
                chdir(lastdirectory.c_str());
                lastdirectory = holddirectory; 
                continue;}
            else  {
                filesystem::path holddir = filesystem::current_path();
                string holddirectory = holddir.c_str();
                chdir(tknr.commands[0]->args[1].c_str());
                lastdirectory = holddirectory;
                continue; }
        }

        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        for (auto cmd : tknr.commands) {
            for (auto str : cmd->args) {
                cerr << "|" << str << "| ";
            }
            if (cmd->hasInput()) {
                cerr << "in< " << cmd->in_file << " ";
            }
            if (cmd->hasOutput()) {
                cerr << "out> " << cmd->out_file << " ";
            }
            cerr << endl;
        }

        //for piping
        //for cmd, : commands
        // call pipe() to make pipe
        // (fork already wrriten) fork() in child, redirect stdout, in par, redirect stdin
        // add checks for first/last command
        for(unsigned int i = 0; i < tknr.commands.size(); i++)  {
            int pipe_fd[2];  //pipe_fd is an array of capacity 2, 0 is reading, 1 is writing


            if (pipe(pipe_fd) == -1) {
            perror("Pipe creation failed"); 
            return 1;
                }

            pid_t childPid1; //, childPid2; 

            //fork, child given pid 
            childPid1 = fork();
            //if child
            if (childPid1 == 0) { // Inside the first child process
                if (i < tknr.commands.size() - 1)    { //last output continues to go to the terminal
                    dup2(pipe_fd[1], 1); //duplicating the file descriptor at the end of the pipe to stdout(1), redirect output to write end
                }
                auto file = tknr.commands[i];

                    //if has input
                if (file->hasInput()) {
                    //open input file to read
                    int pipe_fd = open(file->in_file.c_str(), O_RDONLY, S_IWUSR | S_IRUSR);
                    if (pipe_fd != -1) {
                    dup2(pipe_fd,0);
                    close(pipe_fd);}}

                if (file->hasOutput()){
                    int pipe_fd = open(file->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); 
                    if (pipe_fd != -1) {
                    dup2(pipe_fd,1);
                    close(pipe_fd);}}
                


                close(pipe_fd[0]); //closing read end
                
                vector<string> arguments;
                //good ol vector for loop without knowing size
                for (const string& argument : tknr.commands[i]->args) {
                    arguments.push_back(argument);}
                //vector of char*'s
                vector<char*> argv;
                for (const string& argument : arguments) {
                    char* element = const_cast<char*>(argument.c_str());
                    argv.push_back(element);}

                //last value
                argv.push_back(nullptr);

                execvp(argv[0], argv.data());

                perror("execvp (cmd1) failed");
                return 1;}
            
            else{
                //redirect shell(parent) input to the read end of the pipe
                dup2(pipe_fd[0], 0); //
                close(pipe_fd[1]); //closing the write end of the pipe
                //close le pipe
                //close pipe
                //wait until last command finish, parent 
                if (i == tknr.commands.size()-1)   {
                    if (tknr.commands[i]->isBackground())   { //do not want to wait if background
                        pids.push_back(childPid1);
                        }//}
                    else    {waitpid(childPid1,&status, 0);}
            
                    }}
            close(pipe_fd[0]);
            close(pipe_fd[1]);

            }
           dup2(inp,0);
           dup2(outp,1);
            }}

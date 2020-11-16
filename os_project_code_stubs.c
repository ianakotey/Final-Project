#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

#define NTHREADS 10


//creating a global array to keep track of the commands that are being provided by the user
char *commands[900];
char *directories[900];
int num_of_commands = 0;
int num_elements_command = 0;

//the commandstruct structure is meant to contain the number of commands and the array of commands
typedef struct __mycommands{
	
	} __mycommands;

//concat() function is meant to concatenate 2 strings(i.e path and filename) into 1 string.
char *concat(char *s1,char *s2){
   
	}


//path() provides different paths that a file can be located in and executed.
void path(){
	
	}
	
	
	
//cd() changes the currennt directory of the wish bash shell
void cd(char *d){
	
	}
	
	
//strremove() takes out all of the newline characters from the inputs given by the user
char *strremove(char *str, const char *sub) {
    
	}



//execute_file() is used to execute the commands provided by the user
int execute_file(){
	
	}

//the execute_command() function is responsble for executing the command and the parameters passed into the function.
void *execute_command(void *arg){
	
	}

//check_file() function checks to see if a file can be found in any of the directories and if it is executable
int check_file(char *c){
	
	}

//parallel_check_file() is responsible for checking whether a file provided through parallel commands is executable.
int parallel_check_file(char *c){
	
	}



//stringInput() function splits the input provided by the user into tokens and places them in an array
int split_input(char *s){
	
	}

//the check_redirect() function checks to see whether the commands that the user provides contains '>'
int check_redirect(){
	
	}

//the redirect() function performs the redirects the output of the commands provided into the given file
void redirection(){
		
	}

//the check_parallel() function is responsible to checkking if the command provided by the user involves the execution of parallel commands
int check_parallel(){
	
	}
	


	
//the parallel_commands() function is responsible for performing the parallel commands provided by the user
int parallel_commands(){
	
	}
	
//the main function of the wish command is going to simulate a shell program.
int main(int argc,char *argv[]){
	
	}

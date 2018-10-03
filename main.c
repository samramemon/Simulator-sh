#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/****************    Function Prototypes   ********************/

int printBanner(void);
int quit(void);
int hasSpace(char *string);
int parseLine(void);
int init(void);
int resetEnv(void);
int sysCall(void);
int removeNewLine(char *string);
int hasPipe(char *string);
int hasRedirect(char *string);

/*****************    Global variables    **********************/

char line[128];
char command[16];
char *arguments[10];					// Dynamic list of indiviudal argument strings
char argument[112];					// Single string with all arguments
char *env[] = { "HOME=/user/home", (char *)0 }; 	// environment for execve
char redirectFile[128];					// File name used in case of IO redirectioni
char redirectFileH[128];				// Redirect file for head argument
char redirectFileT[128];				// Redirect file for tail argument
char *head;						// head of piped argument
char *tail;						// tail of piped argument
char *headArgs[10];					// Dynamic list of individual argument strings from the head argument
char *tailArgs[10];					// Dynamic list of individual argument strings from the head argument

/***********************   Main   ******************************/

int main(void)
{
	printBanner();
	
	while (1)					// Keep spawning input line until user quits
	{
		resetEnv();				// Reset globals so last command doesn't interfere with this one		

		printf("User > ");			// Prompt for input, push into line array
		fgets(line, 128, stdin);	
		removeNewLine(line);			// Cut the newline char off the end of the input line				
	
		if (!hasSpace(line))			// If the input string has no space
		{
			strcpy(command, line);		// If there's no space, the whole line should be the command
			arguments[0] = (char *)malloc(strlen(line));
			strcpy(arguments[0], line);
			
			sysCall();			// Send the line to system, should only have one command in it, system will handle error	
			continue;
		}
		
		parseLine();				// Populate arguments[] and command string based on input line
		sysCall();				// Send the argument to system, will make appropriate syscall if one exists
	}
	return 0;
}

/*****************   Helper Functions   **********************/

int sysCall(void)						// Call system giving it argument, with additional cases for simple commands 
{
	char commandline[128];
	int pid, ret, fd, i = 0;
	char binPath[128] = "/bin/";
	char binPath2[128] = "/bin/";

	memset(commandline, 0, 128);				// clear it, there's something in it for some reason :(

	strcat(commandline, command);
	if (hasSpace(line))
	{
		strcat(commandline, " ");
		strcat(commandline, argument);
	}

	if (strcmp(commandline, "cd") == 0)			// Handle cd
	{
		if (hasSpace(line))
		{
			chdir(commandline);
			return 1;
		}	
		chdir(getenv("HOME"));				// If no arguments given with cd, cd to $HOME dir
		return 1;
	}

	if (strcmp(commandline, "exit") == 0)			// Handle exit
	{	
		exit(1);
		return 1;
	}

	/*** Begin Forking ***/

	int status;
	pid = fork();
	if (pid)						// What parent will do
	{
		sleep(1);
		while (wait(&status) > 0) {}
	}
	else							// What child process will do
	{
	        if (hasPipe(line))				// If input is piped, do a seperate fork and pipe process
	        {
	                int pd[2];
	                int pid;
	
        	        strcat(binPath, headArgs[0]);		// Finish the bin path for the head and tail args
        	        strcat(binPath2, tailArgs[0]);

        	        pipe(pd);				// Create a pipe, populates pd with pids
        	        pid = fork();				// fork the piped process

        	        if (pid)        			// parent as pipe writer
        	        {
        	                close(1);
        	                dup(pd[1]);
        	                status = execve(binPath, headArgs, env);
      		      	}
                	else            			// child as pipe reader
                	{
                	        close(0);
                	        dup(pd[0]);
                	        status = execve(binPath2, tailArgs, env);
               		}
			return 1;
		}

		if (hasRedirect(line))				// If no pipe, see if there is an IO Redirection operator. If so, redirect!
		{
			fd = open(redirectFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
			dup2(fd, 1);
			close(fd);
		}
		
		strcat(binPath, command);			// Add command name to the bin path
		status = execve(binPath, arguments, env);		// Execute command
		exit(1);
	}
	return 1;
}

int init(void)
{
	// rip
}

int printBanner(void)
{
	printf("******** Welcome to Samra's sh Simulator *********\n\n");

	return;
}

int quit(void)
{
	exit(0);
}

int hasSpace(char *string)
{
	int i = 0;

	while (string[i])
	{
		if (string[i] == ' ') return 1;
		i++;
	}

	return 0;
}

// Parse user input line
int parseLine(void)
{
	int i = 0, j = 0;
	char *token;
	char tempLine[128];
	char *pipeLine;

	strcpy(tempLine, line);						// Use temporary line string so line will remain intact
	
	if (hasPipe(line))						// If there is a pipe, parse head and tail seperately
	{
		pipeLine = (char *)malloc(128);				// Make another copy of line so the rest of the parsing isn't interfered with (not really necessary but maybe if I end up doing multi piping)
		strcpy(pipeLine, line);
		token = strtok(pipeLine, "|");				// Grab the head
		token[strlen(token) - 1] = 0;				// Cut the space off the end
		head = (char *)malloc(strlen(token));			// Allocate memory for head 
		strcpy(head, token);					// Save the head 

		token = strtok(0, "|");					// Grab the tail
		token++;						// Cut the space off the beginning
		tail = (char *)malloc(strlen(token));			// Allocate memory for tail
		strcpy(tail, token);					// Save the tail

		token = strtok(head, " ");				// Begin tokenizing the head command
       		headArgs[0] = (char *)malloc(strlen(token));		// Allocate first headArg and put command in it
        	strcpy(headArgs[0], token);

        	i = 1; 
        	while (token = strtok(0, " "))					// Tokenize the rest of head
        	{
                	if ((strcmp(token, "<") == 0) || (strcmp(token, ">") == 0) || (strcmp(token, ">>") == 0))
                	{
                        	token = strtok(0, " ");                         // Move to next token
                        	strcpy(redirectFileH, token);                   // Save token after <,>,>> as redirect file name
                        	continue;
                	}

                	headArgs[i] = (char *)malloc(strlen(token));           // Allocate memory for argument string ( 10 args max )
                	strcpy(headArgs[i], token);                            // Copy argument into argument array
                	i++;
        	}

		token = strtok(tail, " ");                              	// Begin tokenizing the tail command
                tailArgs[0] = (char *)malloc(strlen(token));            	// Allocate first headArg and put command in it
                strcpy(tailArgs[0], token);

                i = 1; 
                while (token = strtok(0, " "))                          	// Tokenize the rest of head
                {
                        if ((strcmp(token, "<") == 0) || (strcmp(token, ">") == 0) || (strcmp(token, ">>") == 0))
                        {
                                token = strtok(0, " ");                         // Move to next token
                                strcpy(redirectFileT, token);                   // Save token after <,>,>> as redirect file name
                                continue;
                        }

                       	tailArgs[i] = (char *)malloc(strlen(token));           // Allocate memory for argument string ( 10 args max )
                        strcpy(tailArgs[i], token);                            // Copy argument into argument array
                        i++;
                }
		return 1;
	}

	i = 0;
	while (argument[i] != ' ') i++;                                 // Move to index of first space (pass the command)
        i++;                                                            // Move one more spot, to the start of the arguments
	strcpy(argument, tempLine);						// Copy line into argument ( could possibly have error if line is above 112 chars )
	memmove(argument, argument + i, strlen(argument));		// Shift the argument string down to skip the command	

	token = strtok (tempLine, " ");					// Start tokenizing input line, first one is command
	if (strlen(token) > 16)						// If command is too long for command array, print error and return
	{
		printf("Command too long.\n");
		return 0;
	}

	strcpy(command, token);						// Copy token into command array	
	arguments[0] = (char *)malloc(strlen(token));
	strcpy(arguments[0], token);
	
	i = 1;
	while (token = strtok(0, " "))
	{
		if ((strcmp(token, "<") == 0) || (strcmp(token, ">") == 0) || (strcmp(token, ">>") == 0))
		{
			token = strtok(0, " ");				// Move to next token
			strcpy(redirectFile, token);			// Save token after <,>,>> as redirect file name
			continue;
		}

		arguments[i] = (char *)malloc(strlen(token));		// Allocate memory for argument string ( 10 args max )
		strcpy(arguments[i], token);				// Copy argument into argument array
		i++;
	}
}

// Reset global environment variables
int resetEnv(void)							// Reset globals so old commands don't interfere with new ones
{
	int i = 0;

	memset(line, 0, 128);						// Reset user inputs
	memset(command, 0, 16);
	memset(argument, 0, 112);
	memset(redirectFile, 0, 128);	
	
	while(arguments[i])						// Reset arguments
	{
		memset(arguments[i], 0, strlen(arguments[i]));
		arguments[i] = 0;			 
		i++;
	}

	i = 0;
	while(headArgs[i])						// Reset head arguments
	{
		memset(headArgs[i], 0, strlen(headArgs[i]));
		headArgs[i] = 0;
		i++;
	}

        i = 0;
        while(tailArgs[i])						// Reset tail arguments
        {
                memset(tailArgs[i], 0, strlen(tailArgs[i]));
		tailArgs[i] = 0;
                i++;
        }

	if (head) memset(head, 0, strlen(head));			// Reset head and tail
	if (tail) memset(head, 0, strlen(tail));
}

// Removes the newline character at the end of the given string
int removeNewLine(char *string)
{
	if (string[strlen(string)-1] == '\n')
		string[strlen(string) - 1] = 0;
	return 1;
}

// Returns true if the given string has pipe operator in it
int hasPipe(char *string)
{
	int i = 0;
	
	while (string[i])
	{
		if (string[i] == '|') return 1;
		i++;
	}

	return 0;
}

// Returns true if the given string has an IO redirect operator in it
int hasRedirect(char *string)
{
	int i = 0;

	while (string[i])
	{
		if (string[i] == '>' || string[i] == '<') return 1;
		i++;
	}
	return 0;
}

/*
    Author: Ian Joseph Ayitey Akotey
    This code uses the #pragma directive to avoid unnecessary commenting,
    and to group code in a logical format.
    * eg. #pragma region DoSomething
    *     #pragma endregion // This block surrounds a block that 'Does Something'
*/

#pragma region Library headers
#include <fcntl.h>
#include <regex.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#pragma endregion


#pragma region Constant Declarations
#define pass (void)0
#define DEFAULT_PATH_SIZE (50)
#define MAX_COMMAND_LENGTH (1024)
#define MAX_PATH_LENGTH (256)
#define MAX_DIRECTORY_LENGTH (256)
#pragma endregion Constant Declarations

#pragma region Struct Declarations

/* Token structure.
    Useful for system path and command parameters.
    For example, may be used to hold command fragments
    * eg ['ls', '-la', 'tmp.txt']
*/
typedef struct __token_t {
    size_t size;
    char **tokens;
} token_t;

// Structure for a command
typedef struct __command {
    char *name;
    bool redirect;
    bool builtin;
    char *redirectFile;
    token_t *params;
} command;

// Structure for a command set
typedef struct __commands {
    size_t size;
    command *commands;
} commands;
#pragma endregion Struct Declarations


#pragma region Function Declarations
static inline int setupDefaultEnvironment( void );
int batchMode( const char *fileName );
int interactiveMode( void );
int handleCommands( const char *commandString );
command *createCommand( const char *string, const char *delimiter );
bool isBuiltIn( const char *command );
void *handleBuiltInCommand( void *builtInCommand );
void *handleOtherCommand( void *otherCommand );
void printTokens( token_t *tokens );
int updatePath( command *updatePath );
int changeCurrentDirectory( command *changeCurrentDirectoryCommand );
int builtInExit( command *exitCommand );
int executeCommand( command *command );
void printTheirErrorForThem( );
#pragma endregion Function Declarations


#pragma region Globals
token_t *systemPath;
#pragma endregion Globals


// Comment or uncomment both for normal operation
// #define DEBUG_MODE_INTERACTIVE // Uncomment to enable interactive debugging mode
// #define DEBUG_MODE_BATCH    // Uncomment to enable batch debugging mode


int main( int argc, char *argv [] ) {

    setupDefaultEnvironment( );

    /*
        This region decides the appropriate action of the shell,
        depending on the status of the debug flags provided above main()
        DEBUG_MODE_INTERACTIVE allows interactive mode regardless or arguments passed
        DEBUG_MODE_BATCH does the same for batch mode
        This is to avoid issues with debuggers attaching additional parameters to the process
    */
#pragma region Mode Selector

#if !defined(DEBUG_MODE_BATCH) && defined(DEBUG_MODE_INTERACTIVE)
    printf( "In interactive debug Mode:.............\n" );
    interactiveMode( );

#elif defined(DEBUG_MODE_BATCH) && !defined(DEBUG_MODE_INTERACTIVE)
    printf( "In batch debug Mode:.............\n" );
    batchMode( argv[1] );
#else
    argc == 1 ? interactiveMode( ) : ( argc == 2 ? batchMode( argv[1] ) : printf( "Only one file is needed to call shell.\n" ) );
#endif

#pragma endregion Mode Selector

    return 1;
}

void printTheirErrorForThem( ) {
    fprintf( stderr, "An error has occurred\n" );
}

/* code could have been in main, but this is cleaner */
static inline int setupDefaultEnvironment( void ) {
    /* Setup default system path,
        as well as any other configuration needed
    */
    systemPath = calloc( 1, sizeof( token_t ) );
    systemPath->tokens = calloc( 2, sizeof( char * ) );
    systemPath->tokens[0] = calloc( 5, sizeof( char ) );
    systemPath->tokens[0] = "path";
    systemPath->tokens[1] = calloc( 5, sizeof( char ) );
    systemPath->tokens[1] = "/bin";
    systemPath->size = 2;
}


// Function to count the number of non-empty lines in a file.
size_t countLines( char *fileName ) {
    FILE *file = fopen( fileName, "r" );

    char *lineBuffer = NULL;
    size_t lineBufferSize = 0;
    ssize_t lineLength = 0;

    size_t numLines = 0;

    while ( ( lineLength = getline( &lineBuffer, &lineBufferSize, file ) ) >= 0 ) {

        if ( lineBuffer[0] != '\r' ) { numLines++; } // skip empty lines

        lineLength = getline( &lineBuffer, &lineBufferSize, file );
    }
    fclose( file );
    return numLines;
}

// Function for shell's batch mode
int batchMode( const char *fileName ) {

    FILE *batchFile = fopen( fileName, "r" );

    if ( batchFile == NULL ) {
        exit( 1 );
    }

    char *lineBuffer = NULL;
    size_t lineBufferSize = 0;
    ssize_t lineSize = 0;

    while ( ( lineSize = getline( &lineBuffer, &lineBufferSize, batchFile ) >= 0 ) ) {
        lineBuffer = strtok_r( lineBuffer, "\r\n", &lineBuffer );
        handleCommands( lineBuffer );
    }

    exit( 0 );

}

// Function for shell's interactive mode
int interactiveMode( void ) {

    char *lineBuffer = NULL;
    size_t lineBufferSize = 0;
    ssize_t lineSize = 0;

    printf( "Welcome to wish\n\n" );

    while ( true ) {
        printf( "\nwish>" );
        lineSize = getline( &lineBuffer, &lineBufferSize, stdin );

        lineSize == -1 ? exit( 0 ) : pass; // Handle EOF
        if ( lineSize == 1 ) { printf( "Empty command!\n" );continue; } // Handle \n

        lineBuffer[strlen( lineBuffer ) - 1] = '\0'; // Strip trailing \n
        lineBuffer = strtok_r( lineBuffer, "\n", &lineBuffer );

        handleCommands( lineBuffer );
    }

}


int handleCommands( const char *commandString ) {
    /*
        This function does two things and I hate it.
        It first parses all commands into a commands struct,
        and then spawns threads to handle the commands,
        but this is the most intuitive solution I can come up with ...
        without changing my call stack, which is long enough as-is.
        I've therefore segmented it into logical pieces for easy reading
    */

    size_t num_of_commands = 0;

    char *commandStringCopy = calloc( strlen( commandString ), sizeof( char ) );
    char *commandStringCopyShadow = commandStringCopy; // shadows the pointer above (for freeing sake) 

#pragma region handlecommands.CommandCount
    strcpy( commandStringCopy, commandString );
    char *commandFragment;
    while ( ( commandFragment = strtok_r( commandStringCopy, "&", &commandStringCopy ) ) != NULL ) {
        num_of_commands++;
    }
#pragma endregion handlecommands.CommandCount


#pragma region handlecommands.AllocateCommandsMemory
    commands *passedCommands = calloc( 1, sizeof( commands ) );
    passedCommands->size = 0;
    passedCommands->commands = calloc( num_of_commands, sizeof( command ) );
#pragma endregion

    commandStringCopy = commandStringCopyShadow; // point back to the original memory location
    strcpy( commandStringCopy, commandString );

    commandStringCopy = strtok_r( commandStringCopy, "\r", &commandStringCopy );


#pragma region handlecommands.ParseAllCommands
    while ( ( commandFragment = strtok_r( commandStringCopy, "&", &commandStringCopy ) ) != NULL ) {
        passedCommands->commands[passedCommands->size] = *createCommand( commandFragment, " " );
        passedCommands->commands[passedCommands->size].builtin = isBuiltIn( commandFragment );
        passedCommands->size++;
    }
#pragma endregion

    /*
        Spawn threads to execute the commands in parallel.
        I honestly do not favor this and would prefer sequential execution like in a real UNIX shell.
        The reason is if a command changes directory and a command redirects,
        or two commands redirect their output, there coould be unwanted behaviour.
        The best I can do is synchronize access to the editing of path or redirection.
        ! Edit: This will not work out without blocking other threads,
        ! so parallel at the expense of unpredictability.

    */

#pragma region handlecommands.SpawnThreads
    pthread_t parallel_threads[passedCommands->size];
    for ( size_t index = 0; index < passedCommands->size; index++ ) {
        if ( passedCommands->commands[index].builtin == true ) {
            pthread_create( &parallel_threads[index], NULL,
                            handleBuiltInCommand, ( void * ) &passedCommands->commands[index] );
        } else {
            pthread_create( &parallel_threads[index], NULL,
                            handleOtherCommand, ( void * ) &passedCommands->commands[index] );
        }

    }

    for ( size_t index = 0; index < passedCommands->size; index++ ) {
        pthread_join( parallel_threads[index], NULL );
    }
#pragma endregion


    return 0;

}

bool isBuiltIn( const char *command ) {

    regex_t *commandRegex = calloc( 1, sizeof( regex_t ) );
    int compileStatus = 0;

    compileStatus = regcomp( commandRegex, "^\\s*(path|path .*|cd|cd .*|exit)\\s*$", REG_EXTENDED | REG_NEWLINE );
    bool matched;

    matched = regexec( commandRegex, command, 0, NULL, 0 );

    regfree( commandRegex );
    free( commandRegex );

    return ( matched == 0 ) ? true : false;

}


command *createCommand( const char *string, const char *delimiter ) {
    // return a command* of a command parsed using the delimiter as separator
    char *cloned_string = calloc( strlen( string ), sizeof( char ) );
    strcpy( cloned_string, string );

    char *token = NULL;
    int tokenNumber = 0;


    token = strtok( cloned_string, delimiter );

    command *currentCommand = calloc( 1, sizeof( command ) );
    currentCommand->name = token;
    currentCommand->redirect = false;

    while ( ( token = strtok( NULL, delimiter ) ) != NULL ) {
        tokenNumber++;
    }
    tokenNumber += 2; // One, for the name of the command itself, and one for NULL at the end.


    // initialize the command (Due to how most programs work, the first argument is the program itself)
    currentCommand->params = calloc( 1, sizeof( token_t ) );
    currentCommand->params->tokens = calloc( tokenNumber, sizeof( char * ) );
    currentCommand->params->tokens[0] = calloc( strlen( currentCommand->name ), sizeof( char ) );
    strcpy( currentCommand->params->tokens[0], currentCommand->name );
    currentCommand->params->size = 1;
    int lastTokenIndex = 1;

    strcpy( cloned_string, string );

    token = strtok( cloned_string, delimiter );
    while ( ( token = strtok( NULL, delimiter ) ) != NULL ) {

        if ( currentCommand->redirect == true ) {
            // take this next token as redirection output, then get the hell outta here
            currentCommand->redirectFile = calloc( strlen( token ), sizeof( char ) );
            strcpy( currentCommand->redirectFile, token );

            break;

        } else if ( strcmp( token, ">" ) ) {
            // Store parameters until you find redirection flag
            currentCommand->params->tokens[lastTokenIndex] = calloc( strlen( token ), sizeof( char ) );
            strcpy( currentCommand->params->tokens[lastTokenIndex], token );

            currentCommand->params->size++;
            lastTokenIndex++;

        } else {
            // Setup redirection since the current token seems to be >
            currentCommand->redirect = true;

        }

    }

    currentCommand->params->tokens[lastTokenIndex] = NULL;

    return currentCommand;
}

void printTokens( token_t *tokens ) {

    for ( int index = 0; index < tokens->size; index++ ) {
        printf( "%s\t", tokens->tokens[index] );
    }
    printf( "\n" );

}

#pragma region Built Command Handlers


void *handleBuiltInCommand( void *voidCommand ) {
    command *builtinCommand = ( command * ) voidCommand;
    if ( !strcmp( builtinCommand->name, "path" ) ) {
        updatePath( builtinCommand );
    } else if ( !strcmp( builtinCommand->name, "exit" ) ) {
        builtInExit( builtinCommand );
    } else if ( !strcmp( builtinCommand->name, "cd" ) ) {
        changeCurrentDirectory( builtinCommand );
    }
#if defined(DEBUG_MODE_BATCH) || defined(DEBUG_MODE_INTERACTIVE)
    else if ( !strcmp( builtinCommand->name, "printpath" ) ) {
        printf( "System path: " );
        for ( size_t index = 1; index < systemPath->size; index++ ) {
            printf( "\t%s", systemPath->tokens[index] );
        }
        printf( "\n" );
    }
#endif

}

int builtInExit( command *exitCommand ) {
    exit( 0 );
}

int updatePath( command *updateCommand ) {

    if ( updateCommand != NULL ) {
        // ! Cleanup old path: Providing possible memory leak. To be investigated.
        // for ( int index = 0; index < systemPath->size; index++ )
        //     free( systemPath->tokens[index] );

        free( systemPath->tokens );
        free( systemPath );

        systemPath = updateCommand->params;
    }
    printTokens( systemPath );

}

int changeCurrentDirectory( command *changeCurrentDirectoryCommand ) {
    if ( changeCurrentDirectoryCommand->params->size != 2 ) {
        printf( "Invalid parameter number: %d passed to cd instead of 1", changeCurrentDirectoryCommand->params->size - 1 );
        return 1;
    } else if ( ( chdir( changeCurrentDirectoryCommand->params->tokens[1] ) ) != 0 ) {
        perror( changeCurrentDirectoryCommand->params->tokens[1] );
        printTheirErrorForThem( );
        return 1;
    } else { return 0; }

}
#pragma endregion  Command Handlers


void *handleOtherCommand( void *voidCommand ) {
    command *otherCommand = ( command * ) voidCommand;

    // Step 1: Check if the program works without using path
    if ( access( otherCommand->name, X_OK ) == 0 ) {
        executeCommand( otherCommand );
        return NULL;

    } else {
        // Search for the program in the system path

        for ( int i = 1; i < systemPath->size; i++ ) {
            char *fullPath = calloc( strlen( systemPath->tokens[i] ) + strlen( otherCommand->name ) + 2, sizeof( char ) );
            strcpy( fullPath, systemPath->tokens[i] );
            strcat( fullPath, "/" );
            strcat( fullPath, otherCommand->name );
            if ( access( fullPath, X_OK ) == 0 ) {
                otherCommand->name = fullPath;
                executeCommand( otherCommand );
                return NULL;
            }
        }

    }

    printf( "Command/Executable %s not found.\n", otherCommand->name );
    printTheirErrorForThem( );
    return NULL;
}

int executeCommand( command *command ) {
    /*
        This function executes a command passed.
    */
    // printTokens( command->params );

    int childProcessPID = fork( );
    if ( childProcessPID == -1 ) {
        perror( "Failed to create command process" );
        printTheirErrorForThem( );

    } else if ( childProcessPID == 0 ) {
        int outputFile = open( command->redirectFile, O_CREAT | O_WRONLY );
        if ( command->redirect ) {
            // point Stderr and Stdout to outputFile
            dup2( outputFile, STDOUT_FILENO );
            dup2( outputFile, STDERR_FILENO );
        }

        execv( command->name, command->params->tokens ) < 0 ? printTheirErrorForThem( ) : pass;

        pass;


    } else {
        wait( NULL );
    }

    return 0;

}
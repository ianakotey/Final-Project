
/*
    Author: Ian Joseph Ayitey Akotey
    This code uses the #pragma directive to avoid unnecessary commenting,
    and to group code
*/

#pragma region Library headers
// #include <fcntl.h>
#include <regex.h>
// #include <pthread.h>
// #include <semaphore.h>
#include <stdbool.h>
// #include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <sys/stat.h>
// #include <sys/types.h>
// #include <sys/wait.h>
#include <unistd.h>
#pragma endregion


#pragma region Constant Declarations
#define pass (void)0
#define DEFAULT_PATH_SIZE (50)
#define MAX_COMMAND_LENGTH (1024)
#define MAX_PATH_LENGTH (256)
#define MAX_DIRECTORY_LENGTH (256)
#pragma endregion

#pragma region Struct Declarations

/* Token structure.
    Useful for system path and command parameters.
*/
typedef struct __token_t {
    int size;
    char **tokens;
} token_t;
// Structure for a command
typedef struct __command {
    char *name;
    bool redirect;
    char *redirectFile;
    token_t *params;
} command;
#pragma endregion


#pragma region Function Declarations
static inline int setupDefaultEnvironment( void );
int batchMode( const char *fileName );
int interactiveMode( void );
int handleCommand( const char *command );
command *createCommand( const char *string, const char *delimiter );
bool isBuiltIn( const char *command );
int handleBuiltInCommand( command *builtInCommand );
int handleOtherCommand( command *otherCommand );
void printTokens( token_t *tokens );
int updatePath( command *updatePath );
int changeCurrentDirectory( command *changeCurrentDirectoryCommand );
#pragma endregion


#pragma region Globals
token_t *systemPath;
#pragma endregion

#define DEBUG_MODE_INTERACTIVE // Uncomment to enable interactive debugging mode
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
#ifdef DEBUG_MODE_INTERACTIVE
    interactiveMode( );
#endif

#ifdef  DEBUG_MODE_BATCH
#ifndef DEBUG_MODE_INTERACTIVE
    batchMode( argv[1] );
#endif
#endif

#ifndef DEBUG_MODE_INTERACTIVE
#ifndef  DEBUG_MODE_BATCH
    argc == 1 ? interactiveMode( ) : ( argc == 2 ? batchMode( argv[1] ) : printf( "Only one file is needed to call shell.\n" ) );
#endif
#endif
#pragma endregion

    return 0;
}


/* code could have been in main, but this is cleaner */
static inline int setupDefaultEnvironment( void ) {
    /* Setup default system path,
        as well as any other configuration needed
    */
    systemPath = calloc( 1, sizeof( token_t ) );
    systemPath->tokens = calloc( 1, sizeof( char * ) );
    systemPath->tokens[0] = calloc( 5, sizeof( char ) );
    systemPath->tokens[0] = "/bin";
    systemPath->size = 1;
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
    printf( "Running in batch mode\n" );

    FILE *batchFile = fopen( fileName, "r" );
    char *lineBuffer = NULL;
    size_t lineBufferSize = 0;
    ssize_t lineSize = 0;

    while ( ( lineSize = getline( &lineBuffer, &lineBufferSize, batchFile ) >= 0 ) ) {
        handleCommand( lineBuffer );
    }



}

// Function for shell's interactive mode
int interactiveMode( void ) {
    printf( "Running in interactive mode\n" );


    char *lineBuffer = NULL;
    size_t lineBufferSize = 0;
    ssize_t lineSize = 0;

    printf( "Welcome to wish\nDefault Path:\n" );
    printTokens( systemPath );




    while ( true ) {
        printf( "\nwish>" );
        lineSize = getline( &lineBuffer, &lineBufferSize, stdin );

        lineSize == -1 ? exit( 0 ) : pass; // Handle EOF
        if ( lineSize == 1 ) { printf( "Empty command!\n" );continue; } // Handle \n

        lineBuffer[strlen( lineBuffer ) - 1] = '\0'; // Strip trailing \n

        handleCommand( lineBuffer );
    }

}


int handleCommand( const char *commandString ) {
    command *currentCommand = createCommand( commandString, " " );
    if ( isBuiltIn( commandString ) ) {
        printf( "Built-Command passed\n" );
        handleBuiltInCommand( currentCommand );
    } else {
        printf( "Other defined command passed\n" );
        handleOtherCommand( currentCommand );
    }
}

bool isBuiltIn( const char *command ) {
    regex_t *commandRegex = calloc( 1, sizeof( regex_t ) );
    int compileStatus = 0;
    compileStatus = regcomp( commandRegex, "^(path |cd |exit|printpath|pcwd).*", REG_EXTENDED );
    bool matched;

    matched = regexec( commandRegex, command, 0, NULL, 0 );

    regfree( commandRegex );
    free( commandRegex );
    return ( matched == 0 ) ? true : false;

}


command *createCommand( const char *string, const char *delimiter ) {
    // return a char* array of string parameters splitted by delimiter
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

    currentCommand->params = calloc( 1, sizeof( token_t ) );
    currentCommand->params->tokens = calloc( tokenNumber, sizeof( char * ) );
    int lastTokenIndex = 0;

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

    return currentCommand;
}

void printTokens( token_t *tokens ) {
    printf( "\n" );
    for ( int index = 0; index < tokens->size; index++ ) {
        printf( "%s\t", tokens->tokens[index] );
    }
    printf( "\n" );

}

#pragma region Built Command Handlers
int handleBuiltInCommand( command *command ) {
    if ( !strcmp( command->name, "path" ) ) {
        updatePath( command );
    } else if ( !strcmp( command->name, "exit" ) ) {
        exit( 0 );
    } else if ( !strcmp( command->name, "cd" ) ) {
        changeCurrentDirectory( command );
    } else if ( !strcmp( command->name, "printpath" ) ) {
        printTokens( systemPath );
    } else if ( !strcmp( command->name, "pcwd" ) ) {
        char *cwd = malloc( sizeof( char ) * MAX_DIRECTORY_LENGTH ); // ! magic numbers, but It's a dev only feature so...
        if ( getcwd( cwd, MAX_DIRECTORY_LENGTH ) != NULL )
            printf( "current working directory is: %s\n", cwd );
    }
}

int updatePath( command *updateCommand ) {

    if ( updateCommand != NULL ) {
        // Cleanup old path
        // for ( int index = 0; index < systemPath->size; index++ )
        //     free( systemPath->tokens[index] );

        free( systemPath->tokens );
        free( systemPath );

        systemPath = updateCommand->params;
    }
    printTokens( systemPath );

}

int changeCurrentDirectory( command *changeCurrentDirectoryCommand ) {
    if ( changeCurrentDirectoryCommand->params->size != 1 ) {
        printf( "Invalid parameter number: %d passed to cd instead of 1", changeCurrentDirectoryCommand->params->size );
        return 1;
    } else if ( ( chdir( changeCurrentDirectoryCommand->params->tokens[0] ) ) != 0 ) {
        // printf( "Unable to change current directory to %s\n", changeCurrentDirectoryCommand->params->tokens[0] );
        perror( changeCurrentDirectoryCommand->params->tokens[0] );
        return 1;
    } else { return 0; }

}
#pragma endregion


int handleOtherCommand( command *otherCommand ) {
    // TODO Implement

    // Step 1: Check if the program works without using path
    if ( access( otherCommand->name, F_OK ) == 0 ) {
        printf( "Accessibly without system path\n" );
        return 0;

    } else {
        // Search for the program in the system path

        for ( int i = 0; i < systemPath->size; i++ ) {
            char *fullPath = calloc( strlen( systemPath->tokens[i] ) + strlen( otherCommand->name ) + 2, sizeof( char ) );
            strcpy( fullPath, systemPath->tokens[i] );
            strcat( fullPath, "/" );
            strcat( fullPath, otherCommand->name );
            if ( access( fullPath, F_OK ) == 0 ) {
                printf( "Accessible in %s\n", fullPath );
                return 0;
            }
        }

    }

    printf( "Command/Executable %s not found.\n", otherCommand->name );
    return 1;

}
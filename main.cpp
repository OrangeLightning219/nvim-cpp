#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <stdio.h>
#include <intrin.h>
#include "utils.h"

struct Memory_Arena
{
    memory_index size;
    u8 *base;
    memory_index used;

    s32 tempCount;
};

inline void InitializeArena( Memory_Arena *arena, memory_index size, void *base )
{
    arena->size = size;
    arena->base = ( u8 * ) base;
    arena->used = 0;

    arena->tempCount = 0;
}

#define PushStruct( arena, type )       ( type * ) _PushSize( arena, sizeof( type ) )
#define PushArray( arena, count, type ) ( type * ) _PushSize( arena, count * sizeof( type ) )
#define PushString( arena, size )       ( char * ) _PushSize( arena, size )
#define PushSize( arena, size )         _PushSize( arena, size )
inline void *_PushSize( Memory_Arena *arena, memory_index size )
{
    Assert( arena->used + size <= arena->size );
    void *result = arena->base + arena->used;
    arena->used += size;
    return result;
}

inline void SubArena( Memory_Arena *result, Memory_Arena *parentArena, memory_index size )
{
    result->size = size;
    result->base = ( u8 * ) PushSize( parentArena, size );
    result->used = 0;
    result->tempCount = 0;
}

#include "parser.cpp"

#define DEFAULT_PORT          "12345"
#define DEFAULT_BUFFER_LENGTH 512

enum MP_Type : u8
{
    INVALID,
    // https://github.com/msgpack/msgpack/blob/master/spec.md#formats
    POSITIVE_FIX_INT = 0x00,
    FIX_MAP = 0x80,
    FIX_ARRAY = 0x90,
    FIX_STRING = 0xa0,
    NIL = 0xc0,
    BOOL_FALSE = 0xc2,
    BOOL_TRUE = 0xc3,
    BINARY_8 = 0xc4,
    BINARY_16 = 0xc5,
    BINARY_32 = 0xc6,
    EXT_8 = 0xc7,
    EXT_16 = 0xc8,
    EXT_32 = 0xc9,
    FLOAT_32 = 0xca,
    FLOAT_64 = 0xcb,
    UINT_8 = 0xcc,
    UINT_16 = 0xcd,
    UINT_32 = 0xce,
    UINT_64 = 0xcf,
    INT_8 = 0xd0,
    INT_16 = 0xd1,
    INT_32 = 0xd2,
    INT_64 = 0xd3,
    FIX_EXT_1 = 0xd4,
    FIX_EXT_2 = 0xd5,
    FIX_EXT_4 = 0xd6,
    FIX_EXT_8 = 0xd7,
    FIX_EXT_16 = 0xd8,
    STRING_8 = 0xd9,
    STRING_16 = 0xda,
    STRING_32 = 0xdb,
    ARRAY_16 = 0xdc,
    ARRAY_32 = 0xdd,
    MAP_16 = 0xde,
    MAP_32 = 0xdf,
    NEGATIVE_FIX_INT = 0xe0,
};

struct MP_Parser
{
    u8 *at;
};

internal MP_Type GetType( MP_Parser *parser )
{
    MP_Type result = MP_Type::INVALID;
    u8 byte = *parser->at;
    if ( byte >= 0x00 && byte <= 0x7f ) { result = MP_Type::POSITIVE_FIX_INT; }
    else if ( byte >= 0x80 && byte <= 0x8f ) { result = MP_Type::FIX_MAP; }
    else if ( byte >= 0x90 && byte <= 0x9f ) { result = MP_Type::FIX_ARRAY; }
    else if ( byte >= 0xa0 && byte <= 0xbf ) { result = MP_Type::FIX_STRING; }
    else if ( byte >= 0xe0 && byte <= 0xff ) { result = MP_Type::NEGATIVE_FIX_INT; }
    else
    {
        result = ( MP_Type ) byte;
    }

    return result;
}

internal u32 ParseArrayLength( MP_Parser *parser )
{
    u32 result = UINT32_MAX;
    MP_Type type = GetType( parser );

    switch ( type )
    {
        case MP_Type::FIX_ARRAY:
        {
            result = *parser->at & 0b00001111;
            parser->at += 1;
        }
        break;

        case MP_Type::ARRAY_16:
        {
            result = _byteswap_ushort( *( u16 * ) ( parser->at + 1 ) );
            parser->at += 3;
        }
        break;

        case MP_Type::ARRAY_32:
        {
            result = _byteswap_ulong( *( u32 * ) ( parser->at + 1 ) );
            parser->at += 5;
        }
        break;

            InvalidDefaultCase;
    }

    return result;
}

internal u32 ParseUInt( MP_Parser *parser )
{
    u32 result = UINT32_MAX;
    MP_Type type = GetType( parser );
    switch ( type )
    {
        case MP_Type::POSITIVE_FIX_INT:
        {
            result = *parser->at & 0b01111111;
            parser->at += 1;
        }
        break;

        case MP_Type::UINT_8:
        {
            result = *( parser->at + 1 );
            parser->at += 2;
        }
        break;

        case MP_Type::UINT_16:
        {
            result = _byteswap_ushort( *( u16 * ) ( parser->at + 1 ) );
            parser->at += 3;
        }
        break;

        case MP_Type::UINT_32:
        {
            result = _byteswap_ulong( *( u32 * ) ( parser->at + 1 ) );
            parser->at += 5;
        }
        break;

            InvalidDefaultCase;
    }
    return result;
}

struct String
{
    u32 length;
    char *content;
};

internal String ParseString( MP_Parser *parser )
{
    String result = {};
    MP_Type type = GetType( parser );
    switch ( type )
    {
        case MP_Type::FIX_STRING:
        {
            result.length = *parser->at & 0b00011111;
            result.content = ( char * ) ( parser->at + 1 );
            parser->at += result.length + 1;
        }
        break;

        case MP_Type::STRING_8:
        {
            result.length = *( parser->at + 1 );
            result.content = ( char * ) ( parser->at + 2 );
            parser->at += result.length + 2;
        }
        break;

        case MP_Type::STRING_16:
        {
            result.length = _byteswap_ushort( *( u16 * ) ( parser->at + 1 ) );
            result.content = ( char * ) ( parser->at + 3 );
            parser->at += result.length + 3;
        }
        break;

        case MP_Type::STRING_32:
        {
            result.length = _byteswap_ulong( *( u32 * ) ( parser->at + 1 ) );
            result.content = ( char * ) ( parser->at + 5 );
            parser->at += result.length + 5;
        }
        break;

            InvalidDefaultCase;
    }

    return result;
}

struct MP_Encoder
{
    u8 *at;
    u32 length;
};

internal void EncodeUInt( u32 value, MP_Encoder *encoder )
{
    if ( value < 128 )
    {
        *( ( u8 * ) encoder->at ) = ( u8 ) value;
        encoder->at += 1;
        encoder->length += 1;
    }
    else if ( value <= 0xFF )
    {
        *encoder->at = MP_Type::UINT_8;
        encoder->at += 1;
        *( ( u8 * ) encoder->at ) = ( u8 ) value;
        encoder->at += 1;
        encoder->length += 2;
    }
    else if ( value <= 0xFFFF )
    {
        *encoder->at = MP_Type::UINT_16;
        encoder->at += 1;
        *( ( u16 * ) encoder->at ) = _byteswap_ushort( ( u16 ) value );
        encoder->at += 2;
        encoder->length += 3;
    }
    else
    {
        *encoder->at = MP_Type::UINT_32;
        encoder->at += 1;
        *( ( u32 * ) encoder->at ) = _byteswap_ulong( ( u32 ) value );
        encoder->at += 4;
        encoder->length += 5;
    }
}

internal void EncodeNil( MP_Encoder *encoder )
{
    *encoder->at = MP_Type::NIL;
    encoder->at += 1;
    encoder->length += 1;
}

internal void EncodeMap( u32 length, MP_Encoder *encoder )
{
    if ( length < 16 )
    {
        *encoder->at = MP_Type::FIX_MAP | ( u8 ) length;
        encoder->at += 1;
        encoder->length += 1;
    }
    else if ( length <= 0xFFFF )
    {
        *encoder->at = MP_Type::MAP_16;
        encoder->at += 1;
        *( ( u16 * ) encoder->at ) = _byteswap_ushort( ( u16 ) length );
        encoder->at += 2;
        encoder->length += 3;
    }
    else
    {
        *encoder->at = MP_Type::MAP_32;
        encoder->at += 1;
        *( ( u32 * ) encoder->at ) = _byteswap_ulong( ( u32 ) length );
        encoder->at += 4;
        encoder->length += 5;
    }
}

internal void EncodeArray( u32 length, MP_Encoder *encoder )
{
    if ( length < 16 )
    {
        *encoder->at = MP_Type::FIX_ARRAY | ( u8 ) length;
        encoder->at += 1;
        encoder->length += 1;
    }
    else if ( length <= 0xFFFF )
    {
        *encoder->at = MP_Type::ARRAY_16;
        encoder->at += 1;
        *( ( u16 * ) encoder->at ) = _byteswap_ushort( ( u16 ) length );
        encoder->at += 2;
        encoder->length += 3;
    }
    else
    {
        *encoder->at = MP_Type::ARRAY_32;
        encoder->at += 1;
        *( ( u32 * ) encoder->at ) = _byteswap_ulong( ( u32 ) length );
        encoder->at += 4;
        encoder->length += 5;
    }
}

internal void EncodeString( String string, MP_Encoder *encoder )
{
    u64 length = string.length;

    if ( length < 32 )
    {
        *encoder->at = MP_Type::FIX_STRING | ( u8 ) length;
        encoder->at += 1;
        encoder->length += 1;
    }
    else if ( length <= 0xFF )
    {
        *encoder->at = MP_Type::STRING_8;
        encoder->at += 1;
        *encoder->at = ( u8 ) length;
        encoder->at += 1;
        encoder->length += 2;
    }
    else if ( length <= 0xFFFF )
    {
        *encoder->at = MP_Type::STRING_16;
        encoder->at += 1;
        *( ( u16 * ) encoder->at ) = _byteswap_ushort( ( u16 ) length );
        encoder->at += 1;
        encoder->length += 3;
    }
    else
    {
        *encoder->at = MP_Type::STRING_32;
        encoder->at += 1;
        *( ( u32 * ) encoder->at ) = _byteswap_ulong( ( u32 ) length );
        encoder->at += 1;
        encoder->length += 5;
    }

    for ( u64 characterIndex = 0; characterIndex < length; ++characterIndex )
    {
        *encoder->at = ( u8 ) string.content[ characterIndex ];
        encoder->at += 1;
    }
    encoder->length += ( u32 ) length;
}

internal void EncodeString( char *string, MP_Encoder *encoder )
{
    EncodeString( String{ ( u32 ) strlen( string ), string }, encoder );
}

inline void EncodeBool( bool value, MP_Encoder *encoder )
{
    if ( value )
    {
        *encoder->at = MP_Type::BOOL_TRUE;
    }
    else
    {
        *encoder->at = MP_Type::BOOL_FALSE;
    }
    encoder->at += 1;
    encoder->length += 1;
}

inline bool StringsAreEqual( String a, char *b )
{
    char *at = b;
    for ( u64 index = 0; index < a.length; ++index, ++at )
    {
        if ( *at == '\0' || a.content[ index ] != *at )
        {
            return false;
        }
    }

    bool result = *at == '\0';
    return result;
}

inline u32 StringToUInt( Token string )
{
    u32 result = 0;
    for ( u32 i = 0; i < string.textLength; ++i )
    {
        result += ( u32 ) ( pow( 10, ( f64 ) ( string.textLength - 1 - i ) ) ) * ( u32 ) ( string.text[ i ] - '0' );
    }
    return result;
}

inline u32 StringToUInt( String string )
{
    u32 result = 0;
    for ( u32 i = 0; i < string.length; ++i )
    {
        result += ( u32 ) ( pow( 10, ( f64 ) ( string.length - 1 - i ) ) ) * ( u32 ) ( string.content[ i ] - '0' );
    }
    return result;
}

int main()
{
    void *memoryBase = calloc( Megabytes( 200 ), 1 );
    Memory_Arena arena;
    InitializeArena( &arena, Megabytes( 200 ), memoryBase );

    WSADATA wsaData;
    int result = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
    if ( result != 0 )
    {
        printf( "WSAStartup failed: %d\n", result );
        return 1;
    }

    addrinfo *info;
    addrinfo hints = {};

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    result = getaddrinfo( 0, DEFAULT_PORT, &hints, &info );

    if ( result != 0 )
    {
        printf( "getaddrinfo failed %d\n", result );
        WSACleanup();
        return 1;
    }

    SOCKET listenSocket = INVALID_SOCKET;

    listenSocket = socket( info->ai_family, info->ai_socktype, info->ai_protocol );

    if ( listenSocket == INVALID_SOCKET )
    {
        printf( "Socket creation failed: %d\n", WSAGetLastError() );
        freeaddrinfo( info );
        WSACleanup();
        return 1;
    }

    result = bind( listenSocket, info->ai_addr, ( int ) info->ai_addrlen );

    if ( result == SOCKET_ERROR )
    {
        printf( "bind failed: %d\n", WSAGetLastError() );
        freeaddrinfo( info );
        closesocket( listenSocket );
        WSACleanup();
        return 1;
    }

    freeaddrinfo( info );

    if ( listen( listenSocket, SOMAXCONN ) == SOCKET_ERROR )
    {
        printf( "Listen failed: %d\n", WSAGetLastError() );
        closesocket( listenSocket );
        WSACleanup();
        return 1;
    }

    SOCKET clientSocket = INVALID_SOCKET;

    clientSocket = accept( listenSocket, 0, 0 );

    if ( clientSocket == INVALID_SOCKET )
    {
        printf( "Accept failed: %d\n", WSAGetLastError() );
        closesocket( listenSocket );
        WSACleanup();
        return 1;
    }

    u8 receiveBuffer[ DEFAULT_BUFFER_LENGTH ] = {};

    u8 *responseBuffer = PushArray( &arena, Megabytes( 3 ), u8 );

    printf( "Listening for messages...\n" );

    MP_Parser parser = {};
    MP_Encoder encoder = {};

    Parse_State *parseState = PushStruct( &arena, Parse_State );

    char currentDirectory[ 256 ] = {};
    GetCurrentDirectory( sizeof( currentDirectory ), currentDirectory );
    bool running = true;
    do
    {
        int bytesReceived = recv( clientSocket, ( char * ) receiveBuffer, sizeof( receiveBuffer ), 0 );

        if ( bytesReceived > 0 )
        {
            parser.at = receiveBuffer;

            u32 arrayLength = ParseArrayLength( &parser );
            Assert( arrayLength == 4 );

            u32 interactionType = ParseUInt( &parser );
            Assert( interactionType == 0 );

            u32 messageId = ParseUInt( &parser );
            String command = ParseString( &parser );

            arrayLength = ParseArrayLength( &parser );

            printf( "Received command: %.*s with %d arguments\n", command.length, command.content, arrayLength );
            if ( arrayLength > 0 )
            {
                // parse arguments
            }

            encoder.at = responseBuffer;
            encoder.length = 0;

            EncodeArray( 4, &encoder );
            EncodeUInt( 1, &encoder );
            EncodeUInt( messageId, &encoder );
            EncodeNil( &encoder );

            if ( StringsAreEqual( command, "Exit" ) )
            {
                EncodeUInt( 0, &encoder );
                printf( "Exit command received!\n" );
                running = false;
            }
            else if ( StringsAreEqual( command, "Compile" ) )
            {
                EncodeMap( 2, &encoder );
                // STARTUPINFO startInfo = {};
                // startInfo.cb = sizeof( info );
                // PROCESS_INFORMATION processInfo = {};
                // bool created = CreateProcess( 0, "build.bat", 0, 0, false, 0, 0, 0, &startInfo, &processInfo );
                SHELLEXECUTEINFO info = {};
                info.cbSize = sizeof( info );
                info.fMask = SEE_MASK_NOCLOSEPROCESS;
                info.lpVerb = "open";
                info.lpFile = "build.bat";
                info.lpParameters = "> compilation.log";
                // info.nShow = SW_SHOWNORMAL;
                info.nShow = SW_HIDE;
                bool started = ShellExecuteEx( &info );
                EncodeString( "started", &encoder );
                EncodeBool( started, &encoder );
                EncodeString( "messages", &encoder );
                u8 *arrayCountSpot = encoder.at;
                EncodeArray( 1000, &encoder );

                if ( started )
                {
                    WaitForSingleObject( info.hProcess, INFINITE );

                    char *compilationFile = ReadEntireFileIntoMemoryAndNullTerminate( "compilation.log" );
                    if ( compilationFile )
                    {
                        Tokenizer tokenizer = {};
                        tokenizer.at = compilationFile;
                        bool parsing = true;
                        u32 errorCount = 0;
                        while ( parsing )
                        {
                            Token token = GetToken( &tokenizer );
                            switch ( token.type )
                            {
                                case Token_Type::Colon:
                                {
                                    Token nextToken = GetToken( &tokenizer );
                                    if ( nextToken.type == Token_Type::Identifier && ( TokenEquals( nextToken, "error" ) ||
                                                                                       TokenEquals( nextToken, "fatal" ) ||
                                                                                       TokenEquals( nextToken, "warning" ) ) )
                                    {
                                        char *temp = tokenizer.at;
                                        while ( *temp != '\n' )
                                        {
                                            --temp;
                                        }
                                        ++temp;

                                        Tokenizer errorTokenizer = {};
                                        errorTokenizer.at = temp;

                                        char *fileStart = temp;
                                        char *fileEnd = 0;
                                        Token errorToken = GetToken( &errorTokenizer );
                                        if ( TokenEquals( errorToken, "LINK" ) )
                                        {
                                            fileEnd = errorTokenizer.at;
                                        }
                                        else
                                        {
                                            while ( errorToken.type != Token_Type::OpenParen )
                                            {
                                                errorToken = GetToken( &errorTokenizer );
                                            }
                                            fileEnd = errorTokenizer.at - 1;
                                        }

                                        String filename;
                                        filename.content = fileStart;
                                        filename.length = ( u32 ) ( fileEnd - fileStart );

                                        Token line = {};
                                        Token column = {};
                                        if ( errorToken.type == Token_Type::OpenParen )
                                        {
                                            line = GetToken( &errorTokenizer );
                                            // printf( "line %d ", StringToUInt( line ) );
                                            errorToken = GetToken( &errorTokenizer );
                                            if ( errorToken.type == Token_Type::Comma )
                                            {
                                                column = GetToken( &errorTokenizer );
                                                // printf( "column %d ", StringToUInt( column ) );
                                                errorToken = GetToken( &errorTokenizer );
                                            }

                                            errorToken = GetToken( &errorTokenizer ); // :
                                        }

                                        errorToken = GetToken( &errorTokenizer ); // fatal or error
                                        if ( TokenEquals( errorToken, "fatal" ) )
                                        {
                                            errorToken = GetToken( &errorTokenizer );
                                        }

                                        Token type = errorToken;
                                        Token errorNumber = GetToken( &errorTokenizer );
                                        // printf( "error %.*s ", ( int ) errorNumber.textLength, errorNumber.text );
                                        errorToken = GetToken( &errorTokenizer ); // :

                                        errorToken = GetToken( &errorTokenizer ); // message start
                                        if ( errorToken.type == Token_Type::Character )
                                        {
                                            --errorToken.text;
                                        }
                                        char *messageStart = errorToken.text;
                                        temp = messageStart;
                                        while ( temp && *temp != '\n' )
                                        {
                                            ++temp;
                                        }
                                        --temp;
                                        String message;
                                        message.content = messageStart;
                                        message.length = ( u32 ) ( temp - messageStart );

                                        EncodeMap( 6, &encoder );
                                        EncodeString( "lnum", &encoder );
                                        if ( line.text )
                                        {
                                            EncodeUInt( StringToUInt( line ), &encoder );
                                        }
                                        else
                                        {
                                            EncodeUInt( 1, &encoder );
                                        }

                                        EncodeString( "col", &encoder );
                                        if ( column.text )
                                        {
                                            EncodeUInt( StringToUInt( column ), &encoder );
                                        }
                                        else
                                        {
                                            EncodeUInt( 1, &encoder );
                                        }
                                        EncodeString( "nr", &encoder );
                                        EncodeString( String{ ( u32 ) errorNumber.textLength, errorNumber.text }, &encoder );
                                        EncodeString( "type", &encoder );
                                        if ( TokenEquals( type, "warning" ) )
                                        {
                                            EncodeString( "W", &encoder );
                                        }
                                        else
                                        {
                                            EncodeString( "E", &encoder );
                                        }

                                        EncodeString( "filename", &encoder );
                                        if ( StringsAreEqual( filename, "LINK" ) || StringStartsWith( errorNumber.text, "LNK" ) )
                                        {
                                            EncodeString( "build.bat", &encoder );
                                        }
                                        else
                                        {
                                            EncodeString( filename, &encoder );
                                            // printf( "file: %.*s ", ( int ) filename.length, filename.content );
                                        }
                                        EncodeString( "text", &encoder );
                                        EncodeString( message, &encoder );
                                        // printf( "message %.*s\n", ( int ) message.length, message.content );

                                        tokenizer.at = temp;
                                        errorCount += 1;
                                    }
                                }
                                break;

                                case Token_Type::EndOfStream:
                                {
                                    parsing = false;
                                }
                                break;
                            }
                        }

                        *arrayCountSpot = MP_Type::ARRAY_16;
                        arrayCountSpot += 1;
                        *( ( u16 * ) arrayCountSpot ) = _byteswap_ushort( ( u16 ) errorCount );
                        VirtualFree( compilationFile, 0, MEM_RELEASE );
                        DeleteFile( "compilation.log" );
                    }
                    else
                    {
                        EncodeString( "messages", &encoder );
                        EncodeArray( 1, &encoder );
                        EncodeMap( 1, &encoder );
                        EncodeString( "text", &encoder );
                        EncodeString( "Failed to open compilation log", &encoder );
                    }
                }
                else
                {
                    printf( "ShellExecute failed: %d\n", GetLastError() );
                    EncodeString( "messages", &encoder );
                    EncodeArray( 1, &encoder );
                    EncodeString( "ShellExecute failed", &encoder );
                }
            }
            else if ( StringsAreEqual( command, "GetDeclarations" ) )
            {
                bool sendUpdate = ParseFiles( parseState, &arena, currentDirectory );

                // {
                //     for ( u32 hashIndex = 0; hashIndex < ArrayCount( parseState->filesHash ); ++hashIndex )
                //     {
                //         File_State *file = parseState->filesHash[ hashIndex ];

                //         while ( file )
                //         {
                // printf( "File: %s - %d functions, %d structs, %d macros\n", file->name, file->functionCount, file->structCount, file->macroCount );
                //             Function_Declaration *function = file->functions;
                //             for ( u32 functionIndex = 0; functionIndex < file->functionCount; ++functionIndex )
                //             {
                //                 printf( "%s %s(", function->returnType, function->name );
                //                 for ( u32 argIndex = 0; argIndex < function->argumentCount; ++argIndex )
                //                 {
                //                     Field_Declaration *arg = function->arguments + argIndex;
                //                     printf( "%s %s, ", arg->type, arg->name );
                //                 }
                //                 printf( ")\n" );
                //                 function = function->nextInList;
                //             }

                //             Macro_Declaration *macro = file->macros;
                //             for ( u32 macroIndex = 0; macroIndex < file->macroCount; ++macroIndex )
                //             {
                //                 printf( "#define %s\n", macro->name );
                //                 macro = macro->nextInList;
                //             }

                // Struct_Declaration *structure = file->structs;
                // for ( u32 structIndex = 0; structIndex < file->structCount; ++structIndex )
                // {
                //     printf( "Line %d\n", structure->line );
                //     if ( structure->type == Struct_Type::Struct )
                //     {
                //         printf( "struct " );
                //     }
                //     else if ( structure->type == Struct_Type::Union )
                //     {
                //         printf( "union " );
                //     }
                //     else if ( structure->type == Struct_Type::Enum )
                //     {
                //         printf( "enum " );
                //     }
                //     printf( "%s\n{\n", structure->name );

                //     for ( u32 fieldIndex = 0; fieldIndex < structure->fieldCount; ++fieldIndex )
                //     {
                //         Field_Declaration *field = structure->fields + fieldIndex;
                //         printf( "  %s %s;\n", field->type, field->name );
                //     }
                //     printf( "};\n" );
                //     structure = structure->nextInList;
                // }

                // file = file->nextInHash;
                //         }
                //     }
                // }
                if ( sendUpdate )
                {
                    EncodeArray( 2, &encoder );
                    EncodeBool( sendUpdate, &encoder );

                    EncodeMap( parseState->fileCount, &encoder );
                    {
                        for ( u32 hashIndex = 0; hashIndex < ArrayCount( parseState->filesHash ); ++hashIndex )
                        {
                            File_State *file = parseState->filesHash[ hashIndex ];

                            while ( file )
                            {
                                EncodeString( file->name, &encoder );
                                EncodeMap( 3, &encoder );
                                {
                                    EncodeString( "functions", &encoder );
                                    EncodeArray( file->functionCount, &encoder );
                                    {
                                        Function_Declaration *function = file->functions;
                                        for ( u32 functionIndex = 0; functionIndex < file->functionCount; ++functionIndex )
                                        {
                                            EncodeMap( 4, &encoder );

                                            EncodeString( "line", &encoder );
                                            EncodeUInt( function->line, &encoder );

                                            EncodeString( "name", &encoder );
                                            EncodeString( function->name, &encoder );

                                            EncodeString( "return_type", &encoder );
                                            EncodeString( function->returnType, &encoder );

                                            EncodeString( "arguments", &encoder );
                                            EncodeArray( function->argumentCount, &encoder );
                                            for ( u32 argIndex = 0; argIndex < function->argumentCount; ++argIndex )
                                            {
                                                Field_Declaration *arg = function->arguments + argIndex;
                                                EncodeMap( 2, &encoder );

                                                EncodeString( "type", &encoder );
                                                EncodeString( arg->type, &encoder );

                                                EncodeString( "name", &encoder );
                                                EncodeString( arg->name, &encoder );
                                            }
                                            function = function->nextInList;
                                        }
                                    }

                                    EncodeString( "structs", &encoder );
                                    EncodeArray( file->structCount, &encoder );
                                    {
                                        Struct_Declaration *structure = file->structs;
                                        for ( u32 structIndex = 0; structIndex < file->structCount; ++structIndex )
                                        {
                                            char *type = "struct";
                                            if ( structure->type == Struct_Type::Union )
                                            {
                                                type = "union";
                                            }
                                            else if ( structure->type == Struct_Type::Enum )
                                            {
                                                type = "enum";
                                            }

                                            EncodeMap( 4, &encoder );

                                            EncodeString( "line", &encoder );
                                            EncodeUInt( structure->line, &encoder );

                                            EncodeString( "name", &encoder );
                                            EncodeString( structure->name, &encoder );

                                            EncodeString( "type", &encoder );
                                            EncodeString( type, &encoder );

                                            EncodeString( "fields", &encoder );
                                            EncodeArray( structure->fieldCount, &encoder );
                                            for ( u32 fieldIndex = 0; fieldIndex < structure->fieldCount; ++fieldIndex )
                                            {
                                                Field_Declaration *field = structure->fields + fieldIndex;
                                                EncodeMap( 2, &encoder );

                                                EncodeString( "type", &encoder );
                                                if ( field->type )
                                                {
                                                    EncodeString( field->type, &encoder );
                                                }
                                                else
                                                {
                                                    EncodeNil( &encoder );
                                                }

                                                EncodeString( "name", &encoder );
                                                EncodeString( field->name, &encoder );
                                            }
                                            structure = structure->nextInList;
                                        }
                                    }

                                    EncodeString( "macros", &encoder );
                                    EncodeArray( file->macroCount, &encoder );
                                    {
                                        Macro_Declaration *macro = file->macros;
                                        for ( u32 macroIndex = 0; macroIndex < file->macroCount; ++macroIndex )
                                        {
                                            EncodeMap( 2, &encoder );

                                            EncodeString( "line", &encoder );
                                            EncodeUInt( macro->line, &encoder );

                                            EncodeString( "name", &encoder );
                                            EncodeString( macro->name, &encoder );

                                            macro = macro->nextInList;
                                        }
                                    }
                                }
                                file = file->nextInHash;
                            }
                        }
                    }
                }
                else
                {
                    EncodeMap( 1, &encoder );
                    {
                        EncodeString( "updated", &encoder );
                        EncodeBool( sendUpdate, &encoder );
                    }
                }
            }

            //send response
            int bytesSent = send( clientSocket, ( char * ) responseBuffer, encoder.length, 0 );

            if ( bytesSent == SOCKET_ERROR )
            {
                printf( "Send failed: %d\n", WSAGetLastError() );
                closesocket( clientSocket );
                WSACleanup();
                return 1;
            }
            else
            {
                printf( "Response sent!\n" );
            }
        }
    } while ( running );

    result = shutdown( clientSocket, SD_SEND );

    if ( result == SOCKET_ERROR )
    {
        printf( "Shutdown failed: %d\n", WSAGetLastError() );
        closesocket( clientSocket );
        WSACleanup();
        return 1;
    }

    closesocket( clientSocket );
    WSACleanup();

    return 0;
}

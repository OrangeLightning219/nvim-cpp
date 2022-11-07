#include <math.h>

internal char *ReadEntireFileIntoMemoryAndNullTerminate( char *filename )
{
    void *result = 0;
    HANDLE fileHandle = CreateFile( filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0 );
    if ( fileHandle != INVALID_HANDLE_VALUE )
    {
        LARGE_INTEGER fileSize;
        if ( GetFileSizeEx( fileHandle, &fileSize ) )
        {
            u32 fileSize32 = SafeTruncateU64( fileSize.QuadPart );
            result = VirtualAlloc( 0, fileSize32 + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            if ( result )
            {
                DWORD bytesRead;
                if ( ReadFile( fileHandle, result, fileSize32, &bytesRead, 0 ) && bytesRead == fileSize32 )
                {
                    ( ( char * ) result )[ fileSize32 ] = '\0';
                }
                else
                {
                    VirtualFree( result, 0, MEM_RELEASE );
                }
            }
        }
        CloseHandle( fileHandle );
    }
    return ( char * ) result;
}

enum class Token_Type
{
    OpenParen,
    CloseParen,
    Colon,
    Semicolon,
    Comma,
    Asterisk,
    OpenBracket,
    CloseBracket,
    OpenBrace,
    CloseBrace,
    Preprocessor,
    Equals,

    Identifier,
    Number,
    String,
    Character,

    Unknown,
    EndOfStream
};

struct Token
{
    Token_Type type;
    char *text;
    u64 textLength;
};

struct Tokenizer
{
    char *at;
    u32 lineCount;
};

inline bool StringStartsWith( char *string, char *prefix )
{
    while ( *prefix )
    {
        if ( *prefix != *string )
        {
            return false;
        }
        ++prefix;
        ++string;
    }
    return true;
}

inline bool TokenEquals( Token token, char *string )
{
    char *at = string;
    for ( u64 index = 0; index < token.textLength; ++index, ++at )
    {
        if ( *at == '\0' || token.text[ index ] != *at )
        {
            return false;
        }
    }

    bool result = *at == '\0';
    return result;
}

inline bool TokenEquals( Token a, Token b )
{
    if ( a.textLength != b.textLength )
    {
        return false;
    }

    for ( u64 index = 0; index < a.textLength; ++index )
    {
        if ( a.text[ index ] != b.text[ index ] )
        {
            return false;
        }
    }

    return true;
}

inline bool IsAlpha( char c )
{
    bool result = ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' );
    return result;
}

inline bool IsNumber( char c )
{
    bool result = c >= '0' && c <= '9';
    return result;
}

inline bool IsWhitespace( char c )
{
    bool result = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\\';
    return result;
}

internal void EatAllWhitespace( Tokenizer *tokenizer )
{
    for ( ;; )
    {
        if ( tokenizer->at[ 0 ] == '\n' )
        {
            tokenizer->lineCount += 1;
        }
        if ( IsWhitespace( tokenizer->at[ 0 ] ) )
        {
            ++tokenizer->at;
        }
        else if ( tokenizer->at[ 0 ] == '/' && tokenizer->at[ 1 ] == '/' )
        {
            tokenizer->at += 2;
            while ( tokenizer->at[ 0 ] && tokenizer->at[ 0 ] != '\n' )
            {
                ++tokenizer->at;
            }
        }
        else if ( tokenizer->at[ 0 ] == '/' && tokenizer->at[ 1 ] == '*' )
        {
            tokenizer->at += 2;
            while ( tokenizer->at[ 0 ] )
            {
                if ( tokenizer->at[ 0 ] == '\n' ) { tokenizer->lineCount += 1; }

                if ( tokenizer->at[ 0 ] == '*' && tokenizer->at[ 1 ] == '/' )
                {
                    tokenizer->at += 2;
                    break;
                }
                ++tokenizer->at;
            }
        }
        else
        {
            break;
        }
    }
}

internal Token GetToken( Tokenizer *tokenizer )
{
    EatAllWhitespace( tokenizer );

    Token result = {};
    result.textLength = 1;
    result.text = tokenizer->at;
    char c = tokenizer->at[ 0 ];
    ++tokenizer->at;
    switch ( c )
    {
        case '(': result.type = Token_Type::OpenParen; break;
        case ')': result.type = Token_Type::CloseParen; break;
        case '[': result.type = Token_Type::OpenBracket; break;
        case ']': result.type = Token_Type::CloseBracket; break;
        case '{': result.type = Token_Type::OpenBrace; break;
        case '}': result.type = Token_Type::CloseBrace; break;
        case '*': result.type = Token_Type::Asterisk; break;
        case ';': result.type = Token_Type::Semicolon; break;
        case ',': result.type = Token_Type::Comma; break;
        case ':': result.type = Token_Type::Colon; break;
        case '=': result.type = Token_Type::Equals; break;
        case '\0': result.type = Token_Type::EndOfStream; break;

        case '#':
        {
            result.type = Token_Type::Preprocessor;
            while ( tokenizer->at[ 0 ] && IsAlpha( tokenizer->at[ 0 ] ) )
            {
                ++tokenizer->at;
            }
            result.textLength = tokenizer->at - result.text;
        }
        break;

        case '\'':
        {
            result.type = Token_Type::Character;
            result.text = tokenizer->at;

            if ( tokenizer->at[ 0 ] == '\\' && tokenizer->at[ 1 ] )
            {
                ++tokenizer->at;
            }
            ++tokenizer->at;
            result.textLength = tokenizer->at - result.text;

            if ( tokenizer->at[ 0 ] == '\'' )
            {
                ++tokenizer->at;
            }
        }
        break;

        case '"':
        {
            result.type = Token_Type::String;
            result.text = tokenizer->at;
            while ( tokenizer->at[ 0 ] && tokenizer->at[ 0 ] != '"' )
            {
                if ( tokenizer->at[ 0 ] == '\\' && tokenizer->at[ 1 ] )
                {
                    ++tokenizer->at;
                }
                ++tokenizer->at;
            }
            result.textLength = tokenizer->at - result.text;

            if ( tokenizer->at[ 0 ] == '"' )
            {
                ++tokenizer->at;
            }
        }
        break;

        default:
        {
            if ( IsAlpha( c ) || c == '_' )
            {
                result.type = Token_Type::Identifier;
                while ( tokenizer->at[ 0 ] && ( IsAlpha( tokenizer->at[ 0 ] ) ||
                                                IsNumber( tokenizer->at[ 0 ] ) ||
                                                tokenizer->at[ 0 ] == '_' ) )
                {
                    ++tokenizer->at;
                }
                result.textLength = tokenizer->at - result.text;
            }
            else if ( IsNumber( c ) )
            {
                result.type = Token_Type::Number;
                while ( IsNumber( tokenizer->at[ 0 ] ) )
                {
                    ++tokenizer->at;
                }

                if ( tokenizer->at[ 0 ] == '.' || tokenizer->at[ 0 ] == 'b' )
                {
                    while ( IsNumber( tokenizer->at[ 0 ] ) )
                    {
                        ++tokenizer->at;
                    }
                }
                else if ( tokenizer->at[ 0 ] == 'x' )
                {
                    while ( IsNumber( tokenizer->at[ 0 ] ) || IsAlpha( tokenizer->at[ 0 ] ) )
                    {
                        ++tokenizer->at;
                    }
                }
                result.textLength = tokenizer->at - result.text;
            }
            else
            {
                result.type = Token_Type::Unknown;
            }
        }
        break;
    }

    return result;
}

struct Field_Declaration
{
    char *type;
    char *name;
};

enum class Struct_Type
{
    Struct,
    Enum,
    Union
};

struct Struct_Declaration
{
    char *file;
    u32 line;

    Struct_Type type;
    char *name;

    u32 fieldCount;
    Field_Declaration *fields;

    Struct_Declaration *nextInList;
};

struct Function_Declaration
{
    char *file;
    u32 line;

    char *name;
    char *returnType;

    u32 argumentCount;
    Field_Declaration *arguments;

    Function_Declaration *nextInList;
};

struct Macro_Declaration
{
    char *file;
    u32 line;

    char *name;

    Macro_Declaration *nextInList;
};

struct File_State
{
    Memory_Arena arena;
    char *name;
    FILETIME lastWrite;

    u32 structCount = 0;
    Struct_Declaration *structs;

    u32 functionCount = 0;
    Function_Declaration *functions;

    u32 macroCount = 0;
    Macro_Declaration *macros;

    File_State *nextInHash;
};

struct Parse_State
{
    u32 fileCount;
    File_State *filesHash[ 32 ];
};

inline bool StringsAreEqual( char *a, char *b )
{
    char *at = b;
    for ( u64 index = 0; index < strlen( a ); ++index, ++at )
    {
        if ( *at == '\0' || a[ index ] != *at )
        {
            return false;
        }
    }

    bool result = *at == '\0';
    return result;
}

inline u32 HashString( char *string, u32 length )
{
    u32 result = 0;

    for ( u32 i = 0; i < length; ++i )
    {
        result = 31 * result + string[ i ];
    }

    return result;
}

inline u32 HashString( char *string )
{
    return HashString( string, ( u32 ) strlen( string ) );
}

inline char *PushAndCopyString( Memory_Arena *arena, Token string )
{
    char *result = PushString( arena, string.textLength + 1 );
    strncpy_s( result, string.textLength + 1, string.text, string.textLength );
    result[ string.textLength ] = '\0';

    return result;
}

internal bool ParseFile( Parse_State *state, Memory_Arena *arena, char *file )
{
    bool fileParsed = false;
    if ( file[ strlen( file ) - 1 ] == '~' )
    {
        // printf( "Skipping backup file %s\n", fd.cFileName );
        return false;
    }

    u32 fileIndex = HashString( file ) % ArrayCount( state->filesHash );
    File_State *fileState = 0;
    for ( File_State *testState = state->filesHash[ fileIndex ]; testState; testState = testState->nextInHash )
    {
        if ( StringsAreEqual( testState->name, file ) )
        {
            fileState = testState;
            break;
        }
    }

    FILETIME writeTime = {};
    if ( !fileState )
    {
        state->fileCount += 1;
        fileState = PushStruct( arena, File_State );
        fileState->nextInHash = state->filesHash[ fileIndex ];
        state->filesHash[ fileIndex ] = fileState;

        u32 pathLength = ( u32 ) strlen( file );
        fileState->name = PushString( arena, pathLength + 1 );
        strcpy_s( fileState->name, pathLength + 1, file );
        fileState->lastWrite = {};

        SubArena( &fileState->arena, arena, Megabytes( 1 ) );
        fileState->structCount = 0;
        fileState->functionCount = 0;
    }

    WIN32_FILE_ATTRIBUTE_DATA data;
    if ( GetFileAttributesEx( file, GetFileExInfoStandard, &data ) )
    {
        writeTime = data.ftLastWriteTime;
        if ( CompareFileTime( &writeTime, &fileState->lastWrite ) != 0 )
        {
            fileState->lastWrite = writeTime;
            fileState->structCount = 0;
            fileState->functionCount = 0;
            fileState->macroCount = 0;
            fileState->arena.used = 0;
            u8 *byte = ( u8 * ) fileState->arena.base;
            memory_index size = fileState->arena.size;
            while ( size-- )
            {
                *byte++ = 0;
            }
        }
        else
        {
            return false;
        }
    }

    printf( "Parsing file %s\n", fileState->name );
    fileParsed = true;

    Struct_Declaration *fileStructs = fileState->structs;
    Function_Declaration *fileFunctions = fileState->functions;

    char *fileContent = ReadEntireFileIntoMemoryAndNullTerminate( file );
    if ( !fileContent )
    {
        printf( "Failed to open file %s\n", file );
        return false;
    }
    Tokenizer tokenizer = {};
    tokenizer.at = fileContent;
    tokenizer.lineCount = 1;

    bool parsing = true;

    u32 previousLineCount = 0;
    while ( parsing )
    {
        Token token = GetToken( &tokenizer );
        switch ( token.type )
        {
            case Token_Type::Preprocessor:
            {
                if ( TokenEquals( token, "#define" ) )
                {
                    Token name = GetToken( &tokenizer );

                    Macro_Declaration *macro = PushStruct( &fileState->arena, Macro_Declaration );
                    macro->line = tokenizer.lineCount;
                    macro->file = fileState->name;
                    macro->name = PushAndCopyString( &fileState->arena, name );
                    // printf( "Macro name %s\n", macro->name );

                    macro->nextInList = fileState->macros;
                    fileState->macros = macro;
                    fileState->macroCount += 1;
                }
                break;

                case Token_Type::Identifier:
                {
                    if ( TokenEquals( token, "typedef" ) )
                    {
                        while ( token.type != Token_Type::Semicolon )
                        {
                            token = GetToken( &tokenizer );
                        }
                    }
                    else if ( TokenEquals( token, "extern" ) || TokenEquals( token, "inline" ) || TokenEquals( token, "internal" ) )
                    {
                        if ( TokenEquals( token, "extern" ) )
                        {
                            token = GetToken( &tokenizer );
                            if ( token.type != Token_Type::String && !TokenEquals( token, "C" ) )
                            {
                                while ( token.type != Token_Type::Semicolon )
                                {
                                    token = GetToken( &tokenizer );
                                }
                                continue;
                            }
                            char *temp = tokenizer.at;
                            token = GetToken( &tokenizer );
                            if ( token.type == Token_Type::Identifier )
                            {
                                tokenizer.at = temp;
                            }
                            else if ( token.type == Token_Type::OpenBrace )
                            {
                                continue;
                            }
                        }
                        char *temp = tokenizer.at;
                        while ( *temp && *temp != ')' )
                        {
                            ++temp;
                        }

                        //process if it's not forward declared
                        if ( temp[ 1 ] != ';' )
                        {
                            Function_Declaration *function = PushStruct( &fileState->arena, Function_Declaration );
                            Token type = GetToken( &tokenizer );
                            Token nextToken = GetToken( &tokenizer );
                            if ( nextToken.type == Token_Type::Asterisk )
                            {
                                type.textLength = nextToken.text - type.text + 1;
                                nextToken = GetToken( &tokenizer );
                            }
                            Token name = nextToken;

                            function->line = tokenizer.lineCount;
                            function->file = fileState->name;
                            function->returnType = PushAndCopyString( &fileState->arena, type );
                            function->name = PushAndCopyString( &fileState->arena, name );
                            // printf( "Function name %s\n", function->name );

                            nextToken = GetToken( &tokenizer ); // open paren
                            nextToken = GetToken( &tokenizer ); // close paren if no arguments

                            Tokenizer counter = tokenizer;
                            Token counterToken = nextToken;
                            u32 argCount = 0;
                            while ( counterToken.type != Token_Type::CloseParen )
                            {
                                counterToken = GetToken( &counter );
                                if ( counterToken.type == Token_Type::Asterisk )
                                {
                                    counterToken = GetToken( &counter );
                                }

                                argCount += 1;
                                while ( counterToken.type != Token_Type::Comma && counterToken.type != Token_Type::CloseParen )
                                {
                                    counterToken = GetToken( &counter );
                                }

                                if ( counterToken.type == Token_Type::Comma )
                                {
                                    counterToken = GetToken( &counter );
                                }
                            }

                            if ( argCount > 0 )
                            {
                                function->arguments = PushArray( &fileState->arena, argCount, Field_Declaration );
                                while ( nextToken.type != Token_Type::CloseParen )
                                {
                                    Token argType = nextToken;
                                    nextToken = GetToken( &tokenizer );
                                    if ( nextToken.type == Token_Type::Asterisk )
                                    {
                                        argType.textLength = nextToken.text - argType.text + 1;
                                        nextToken = GetToken( &tokenizer );
                                    }
                                    Token argName = nextToken;

                                    Field_Declaration *arg = function->arguments + function->argumentCount++;
                                    arg->type = PushAndCopyString( &fileState->arena, argType );
                                    // printf( "Arg type %s\n", arg->type );
                                    arg->name = PushAndCopyString( &fileState->arena, argName );
                                    // printf( "Arg name %s\n", arg->name );

                                    while ( nextToken.type != Token_Type::Comma && nextToken.type != Token_Type::CloseParen )
                                    {
                                        nextToken = GetToken( &tokenizer );
                                    }

                                    if ( nextToken.type == Token_Type::Comma )
                                    {
                                        nextToken = GetToken( &tokenizer );
                                    }
                                }
                            }
                            else
                            {
                                function->arguments = 0;
                            }

                            function->nextInList = fileState->functions;
                            fileState->functions = function;
                            fileState->functionCount += 1;
                        }
                    }
                    else if ( TokenEquals( token, "enum" ) )
                    {
                        Token nextToken = GetToken( &tokenizer );

                        if ( nextToken.type == Token_Type::OpenBrace )
                        {
                            while ( nextToken.type != Token_Type::Semicolon )
                            {
                                nextToken = GetToken( &tokenizer );
                            }
                            continue;
                        }
                        Token name;
                        if ( TokenEquals( nextToken, "class" ) )
                        {
                            name = GetToken( &tokenizer );
                        }
                        else
                        {
                            name = nextToken;
                        }

                        nextToken = GetToken( &tokenizer );
                        //process if it's not forward declared
                        if ( nextToken.type != Token_Type::Semicolon )
                        {
                            Struct_Declaration *structure = PushStruct( &fileState->arena, Struct_Declaration );
                            structure->file = fileState->name;
                            structure->line = tokenizer.lineCount - 1;
                            structure->type = Struct_Type::Enum;
                            structure->name = PushAndCopyString( &fileState->arena, name );
                            // printf( "Enum name %s\n", structure->name );

                            while ( nextToken.type != Token_Type::OpenBrace )
                            {
                                nextToken = GetToken( &tokenizer );
                            }

                            nextToken = GetToken( &tokenizer );

                            Tokenizer counter = tokenizer;
                            Token counterToken = nextToken;
                            u32 fieldCount = 0;
                            while ( counterToken.type != Token_Type::CloseBrace )
                            {
                                if ( counterToken.type == Token_Type::Identifier )
                                {
                                    fieldCount += 1;
                                }
                                counterToken = GetToken( &counter );
                            }

                            if ( fieldCount > 0 )
                            {
                                structure->fields = PushArray( &fileState->arena, fieldCount, Field_Declaration );

                                while ( nextToken.type != Token_Type::CloseBrace )
                                {
                                    if ( nextToken.type == Token_Type::Identifier )
                                    {
                                        Field_Declaration *field = structure->fields + structure->fieldCount++;
                                        field->name = PushAndCopyString( &fileState->arena, nextToken );
                                    }
                                    nextToken = GetToken( &tokenizer );
                                }
                            }
                            else
                            {
                                structure->fields = 0;
                            }

                            structure->nextInList = fileState->structs;
                            fileState->structs = structure;
                            fileState->structCount += 1;
                        }
                    }
                    else if ( TokenEquals( token, "struct" ) || TokenEquals( token, "union" ) )
                    {
                        Struct_Type structType;
                        if ( TokenEquals( token, "struct" ) )
                        {
                            structType = Struct_Type::Struct;
                        }
                        else
                        {
                            structType = Struct_Type::Union;
                        }

                        Token name = GetToken( &tokenizer );

                        Token nextToken = GetToken( &tokenizer );

                        // process if it's not forward declared
                        if ( nextToken.type != Token_Type::Semicolon )
                        {
                            Struct_Declaration *structure = PushStruct( &fileState->arena, Struct_Declaration );
                            structure->file = fileState->name;
                            structure->line = tokenizer.lineCount - 1;
                            structure->type = structType;
                            structure->name = PushAndCopyString( &fileState->arena, name );
                            // printf( "Struct name %s\n", structure->name );

                            int openBraces = 1;
                            Tokenizer counter = tokenizer;
                            Token counterToken = nextToken;
                            u32 fieldCount = 0;
                            while ( openBraces > 0 )
                            {
                                counterToken = GetToken( &counter );
                                if ( counterToken.type == Token_Type::OpenBrace )
                                {
                                    openBraces += 1;
                                }
                                else if ( counterToken.type == Token_Type::CloseBrace )
                                {
                                    openBraces -= 1;
                                }
                                else if ( counterToken.type == Token_Type::Identifier &&
                                          !TokenEquals( counterToken, "struct" ) &&
                                          !TokenEquals( counterToken, "union" ) )
                                {
                                    Token counterToken = GetToken( &counter );
                                    if ( counterToken.type == Token_Type::Asterisk )
                                    {
                                        counterToken = GetToken( &counter );
                                    }

                                    if ( counterToken.type == Token_Type::OpenParen ) // skip functions
                                    {
                                        while ( counterToken.type != Token_Type::CloseParen )
                                        {
                                            counterToken = GetToken( &counter );
                                        }
                                        counterToken = GetToken( &counter );
                                        if ( counterToken.type == Token_Type::OpenBrace )
                                        {
                                            while ( counterToken.type != Token_Type::CloseBrace )
                                            {
                                                counterToken = GetToken( &counter );
                                            }
                                        }
                                    }
                                    else
                                    {
                                        fieldCount += 1;
                                        counterToken = GetToken( &counter );
                                        while ( counterToken.type != Token_Type::Semicolon && nextToken.type != Token_Type::Equals )
                                        {
                                            counterToken = GetToken( &counter );
                                        }
                                    }
                                }
                                else if ( counterToken.type == Token_Type::Preprocessor )
                                {
                                    while ( counter.at[ 0 ] && counter.at[ 0 ] != '\n' )
                                    {
                                        ++counter.at;
                                    }
                                }
                            }

                            if ( fieldCount > 0 )
                            {
                                structure->fields = PushArray( &fileState->arena, fieldCount, Field_Declaration );
                                openBraces = 1;
                                while ( openBraces > 0 )
                                {
                                    nextToken = GetToken( &tokenizer );
                                    if ( nextToken.type == Token_Type::OpenBrace )
                                    {
                                        openBraces += 1;
                                    }
                                    else if ( nextToken.type == Token_Type::CloseBrace )
                                    {
                                        openBraces -= 1;
                                    }
                                    else if ( nextToken.type == Token_Type::Identifier &&
                                              !TokenEquals( nextToken, "struct" ) &&
                                              !TokenEquals( nextToken, "union" ) )
                                    {
                                        Token type = nextToken;
                                        Token nextToken = GetToken( &tokenizer );
                                        if ( nextToken.type == Token_Type::Asterisk )
                                        {
                                            type.textLength = nextToken.text - type.text + 1;
                                            nextToken = GetToken( &tokenizer );
                                        }
                                        Token name = nextToken;

                                        if ( name.type == Token_Type::OpenParen ) // skip functions
                                        {
                                            while ( nextToken.type != Token_Type::CloseParen )
                                            {
                                                nextToken = GetToken( &tokenizer );
                                            }
                                            nextToken = GetToken( &tokenizer );
                                            if ( nextToken.type == Token_Type::OpenBrace )
                                            {
                                                while ( nextToken.type != Token_Type::CloseBrace )
                                                {
                                                    nextToken = GetToken( &tokenizer );
                                                }
                                            }
                                        }
                                        else
                                        {
                                            Field_Declaration *field = structure->fields + structure->fieldCount++;
                                            field->name = PushAndCopyString( &fileState->arena, name );
                                            // printf( "Field name %s\n", field->name );

                                            char typeBuffer[ 128 ] = {};
                                            strncpy_s( typeBuffer, sizeof( typeBuffer ), type.text, ( int ) type.textLength );

                                            nextToken = GetToken( &tokenizer );
                                            while ( nextToken.type != Token_Type::Semicolon && nextToken.type != Token_Type::Equals )
                                            {
                                                char buffer[ 128 ] = {};
                                                ConcatenateStrings( typeBuffer, ( int ) strlen( typeBuffer ), nextToken.text, ( int ) nextToken.textLength, buffer );
                                                strncpy_s( typeBuffer, sizeof( typeBuffer ), buffer, strlen( buffer ) );
                                                nextToken = GetToken( &tokenizer );
                                            }
                                            type.text = typeBuffer;
                                            type.textLength = strlen( typeBuffer );
                                            field->type = PushAndCopyString( &fileState->arena, type );
                                            // printf( "Field type %s\n", field->type );
                                        }
                                    }
                                    else if ( nextToken.type == Token_Type::Preprocessor )
                                    {
                                        while ( tokenizer.at[ 0 ] && tokenizer.at[ 0 ] != '\n' )
                                        {
                                            ++tokenizer.at;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                structure->fields = 0;
                            }

                            structure->nextInList = fileState->structs;
                            fileState->structs = structure;
                            fileState->structCount += 1;
                        }
                    }
                }
                break;

                case Token_Type::EndOfStream:
                {
                    parsing = false;
                }
                break;

                default:
                {
                    // printf( "%d: %.*s\n", token.type, ( int ) token.textLength, token.text );
                }
                break;
            }
        }
    }
    VirtualFree( fileContent, 0, MEM_RELEASE );
    return fileParsed;
}

internal int StringEndsWith( const char *string, const char *suffix )
{
    size_t stringLength = strlen( string );
    size_t suffixLength = strlen( suffix );
    if ( suffixLength > stringLength )
    {
        return 0;
    }
    return strncmp( string + stringLength - suffixLength, suffix, suffixLength ) == 0;
}

internal bool ParseFiles( Parse_State *state, Memory_Arena *arena, char *startDirectory )
{
    bool result = false;
    WIN32_FIND_DATA fd = {};
    char *pathToSearch = PushString( arena, 256 );

    sprintf_s( pathToSearch, 256, "%s\\*", startDirectory );
    HANDLE handle = FindFirstFile( pathToSearch, &fd );
    if ( handle != INVALID_HANDLE_VALUE )
    {
        do
        {
            if ( ( strncmp( ".", fd.cFileName, 1 ) != 0 ) && ( strncmp( "..", fd.cFileName, 2 ) != 0 ) )
            {
                if ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
                {
                    sprintf_s( pathToSearch, 256, "%s\\%s", startDirectory, fd.cFileName );
                    bool wasParsed = ParseFiles( state, arena, pathToSearch );
                    if ( wasParsed )
                    {
                        result = true;
                    }
                }
                else if ( StringEndsWith( fd.cFileName, ".h" ) || StringEndsWith( fd.cFileName, ".cpp" ) )
                {
                    char *fullFilePath = PushString( arena, 256 );
                    sprintf_s( fullFilePath, 256, "%s\\%s", startDirectory, fd.cFileName );
                    bool wasParsed = ParseFile( state, arena, fullFilePath );
                    if ( wasParsed )
                    {
                        result = true;
                    }
                }
            }
        } while ( FindNextFile( handle, &fd ) );

        FindClose( handle );
    }
    return result;
}


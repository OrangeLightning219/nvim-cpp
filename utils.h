#pragma once

#include <stdint.h>
#include <float.h>

#define global_variable static
#define local_persist   static
#define internal        static
#define pi32            3.14159265359f

typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef size_t memory_index;

#define Float32Max FLT_MAX

#if SLOW
    #define Assert( expression )    \
        if ( !( expression ) )      \
        {                           \
            int *volatile null = 0; \
            *null = 0;              \
        }
#else
    #define Assert( expression )
#endif

#define CONCAT_INTERNAL( x, y ) x##y
#define CONCAT( x, y )          CONCAT_INTERNAL( x, y )

#define InvalidCodePath Assert( !"InvalidCodePath" )
#define InvalidDefaultCase \
    default:               \
    {                      \
        InvalidCodePath;   \
    }                      \
    break

#define Kilobytes( value ) ( ( value ) *1024LL )
#define Megabytes( value ) ( Kilobytes( value ) * 1024LL )
#define Gigabytes( value ) ( Megabytes( value ) * 1024LL )
#define Terabytes( value ) ( Gigabytes( value ) * 1024LL )

#define ArrayCount( array ) ( ( int ) ( sizeof( array ) / sizeof( ( array )[ 0 ] ) ) )

inline u32 SafeTruncateU64( u64 value )
{
    Assert( value <= 0xFFFFFFFF );
    return ( u32 ) value;
}

inline void ConcatenateStrings( char *sourceA, int countA, char *sourceB, int countB, //NOLINT
                                char *destination )
{
    for ( int index = 0; index < countA; ++index )
    {
        *destination++ = *sourceA++;
    }
    for ( int index = 0; index < countB; ++index )
    {
        *destination++ = *sourceB++;
    }
    *destination++ = '\0';
}

inline int StringLength( char *string ) //NOLINT
{
    int length = 0;
    while ( *string++ )
    {
        ++length;
    }
    return length;
}

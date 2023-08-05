
#include "generic.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


// smonta una stringa
// /a/b/c/d -> [ a, b, c, d ]
// il percorso arriva sempre come assoluto rispetto al mountpoint

char ** tokenize( const char * str, char c ) {
    if ( ! str )
        return NULL;

    char ** ret;
    char * s;
    char * copy;
    char * sample;
    size_t i;

    s = str;
    copy = calloc( strlen(str), sizeof(char) ); //strdup(str);
    if ( ! copy )
        goto cleanup;
    memcpy(copy, str, strlen(str));
    
    
    for (i=1; s[i]; s[i]== c ? i++ : *s++); // stima il numero di token, sempre almeno uno


    ret = (char **) calloc( i + 1, sizeof( char * ) );
    if ( ret == NULL )
        goto cleanup;

    // tokenize
    for ( s = sample = copy, i = 0; *s != '\0' ; s++ ) {

        if ( (*s != c) )
            continue;

        ret[i] = sample;
        sample = s + 1;
        i++;
        
        *s = '\0';

    }

    // se il cursore è arrivato a fine stringa senza
    // incontrare un carattere target, memorizza l'ultimo token
    //          /a/b/c/d
    if ( sample != s ){
        ret[i] = sample; 
        i++;
    }

    ret[i] = NULL;

    return ret;

cleanup:

    if ( ret )
        free(ret);

    if ( copy )
        free(copy);

    return NULL;

}


void free_str_arr( const char ** arr ) {

    if ( ! arr )
        return;

    if ( arr[0] != NULL )
        free( arr[0] );
    free( arr );
}




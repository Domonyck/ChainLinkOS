#ifndef SESSION_H
#define SESSION_H


typedef enum {
    MODE_CLASSIC,    
    MODE_BLITZ       
} GameMode;

typedef enum {
    LIST_GAME_MASTER,    
    LIST_PLAYER          
} ListSource;

typedef struct Session Session;

Session *create_session(int fd0, int fd1);

void run_session(Session *s);

void destroy_session(Session *s);

#endif /* SESSION_H */

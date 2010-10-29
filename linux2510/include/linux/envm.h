#ifndef _ENVM_H
#define _ENVM_H

extern int envm_load(void);
extern int envm_init(void);
extern char *envm_read(char *name);
extern int envm_write(char *name, char *value);

#endif   /*_ENVM_H */


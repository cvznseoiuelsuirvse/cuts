#ifndef CUTS_UTIL_SHM
#define CUTS_UTIL_SHM

int new_shm(int size, int *rfd, int *rwfd);
void *new_shm_mmap(int size, int *rfd, int *rwfd, int prot, int flags);

#endif

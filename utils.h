#ifndef _MY_UTILS_H_
#define _MY_UTILS_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>


static inline double getcurrent_ms(){
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int64_t tv_sec = tv.tv_sec*1000;
	
	tv_sec &= 0xffffff;
	double ms = (double)tv_sec + ((double)tv.tv_usec)/1000;
	return ms/1000;
}

#define gettid() syscall(__NR_gettid)


#define DLOG(fmt, arg... )  printf("%10.3f:%5ld:%s:%d  |  " fmt "\n", getcurrent_ms() , gettid(),  __FUNCTION__, __LINE__, ##arg )

#define RECHECK(exp) \
	do{ \
		if(!(exp)){ \
			DLOG("FAILED CHECK %s " ,#exp); \
			return -1; \
		} \
	}while(0)
#define RVCHECK(exp) \
	do{ \
		if(!(exp)){ \
			DLOG("FAILED CHECK %s " ,#exp); \
			return ; \
		} \
	}while(0)

#define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

  
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
  
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName();                                    \
  DISALLOW_COPY_AND_ASSIGN(TypeName)
  
#endif
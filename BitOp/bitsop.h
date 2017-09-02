#ifndef __BITS__
#define __BITS__

#define IS_BIT_SET(n, pos)	(n & (1 << (pos)))
#define TOGGLE_BIT(n, pos)	(n = n ^ (1 << (pos)))
#define UNSET_BIT(n, pos)  	(if(IS_BIT_SET(n, pos))  (n = TOGGLE_BIT(n, pos)))

#define SET_BIT(n, pos)     	\
    if(!IS_BIT_SET(n, pos))     \
        n = TOGGLE_BIT(n, pos)

#define COMPLEMENT(num)	   	(n = num ^ 0xFFFFFFFF)

#endif


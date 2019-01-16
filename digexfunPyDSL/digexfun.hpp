#ifndef _DIGEXFUN_HPP
#define _DIGEXFUN_HPP

// message types for printing to PF output window
#define MSG_NONE   0x0000
#define MSG_ERR    0x0001
#define MSG_WRNG   0x0002
#define MSG_INFO   0x0004
#define MSG_PCL    0x0010

typedef void (__stdcall * FP_PRINTMSG)(const char *, unsigned int);
typedef void (__stdcall * FP_STOPSIM)(void);

/******************************************************************************\
*                              FUNCTION PROTOTYPES
\******************************************************************************/

extern void __stdcall         push(double f);

// push : pushes a double on the double-value stack
extern double __stdcall       pop(void);

// pop : pops and returns the top-most value from the double_stack
extern const char * __stdcall get_wrk_path();

// get working directory
extern void __cdecl           emitMessage(const char *msg);

extern void                   print_pf(const char  *msg,
                                       unsigned int msgType);

extern void                   stop_simulation();

extern void                   LoadDigsiLibrary();

// load library digadm.dll, if not already done

#endif // ifndef _DIGEXFUN_HPP

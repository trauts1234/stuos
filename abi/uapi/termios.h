#ifndef UAPI_TERMIOS_H
#define UAPI_TERMIOS_H

typedef unsigned char	cc_t;
typedef unsigned int	speed_t;
typedef unsigned int	tcflag_t;

struct termios {
    tcflag_t c_iflag;      /* input modes */
    tcflag_t c_oflag;      /* output modes */
    tcflag_t c_cflag;      /* control modes */
    tcflag_t c_lflag;      /* local modes */
    cc_t     c_cc[32];   /* special characters */
};

/* c_cc characters */
// #define VINTR 0
// #define VQUIT 1
// #define VERASE 2
// #define VKILL 3
// #define VEOF 4
// #define VTIME 5
// #define VMIN 6
// #define VSWTC 7
// #define VSTART 8
// #define VSTOP 9
// #define VSUSP 10
// #define VEOL 11
// #define VREPRINT 12
// #define VDISCARD 13
// #define VWERASE 14
// #define VLNEXT 15
// #define VEOL2 16

/* c_iflag bits */
// #define IGNBRK	0000001  /* Ignore break condition.  */
// #define BRKINT	0000002  /* Signal interrupt on break.  */
// #define IGNPAR	0000004  /* Ignore characters with parity erro/rs.  */
// #define PARMRK	0000010  /* Mark parity and framing errors.  */
// #define INPCK	0000020  /* Enable input parity check.  */
// #define ISTRIP	0000040  /* Strip 8th bit off characters.  */
// #define INLCR	0000100  /* Map NL to CR on input.  */
// #define IGNCR	0000200  /* Ignore CR.  */
// #define ICRNL	0000400  /* Map CR to NL on input.  */
// #define IUCLC	0001000  /* Map uppercase characters to lowercase on input (not in POSIX).  */
// #define IXON	0002000  /* Enable start/stop output control.  */
// #define IXANY	0004000  /* Enable any character to restart output.  */
// #define IXOFF	0010000  /* Enable start/stop input control.  */
// #define IMAXBEL	0020000  /* Ring bell when input queue is full (not in POSIX).  */
// #define IUTF8	0040000  /* Input is UTF8 (not in POSIX).  */

/* c_oflag bits */
// #define OPOST	0000001  /* Post-process output.  */
// #define OLCUC	0000002  /* Map lowercase characters to uppercase on output. (not in POSIX).  */
// #define ONLCR	0000004  /* Map NL to CR-NL on output.  */
// #define OCRNL	0000010  /* Map CR to NL on output.  */
// #define ONOCR	0000020  /* No CR output at column 0.  */
// #define ONLRET	0000040  /* NL performs CR function.  */
// #define OFILL	0000100  /* Use fill characters for delay.  */
// #define OFDEL	0000200  /* Fill is DEL.  */

// #define VTDLY	0040000  /* Select vertical-tab delays:  */
// #define   VT0	0000000  /* Vertical-tab delay type 0.  */
// #define   VT1	0040000  /* Vertical-tab delay type 1.  */

/* c_cflag bit meaning */
// #define  B0	0000000		/* hang up */
// #define  B50	0000001
// #define  B75	0000002
// #define  B110	0000003
// #define  B134	0000004
// #define  B150	0000005
// #define  B200	0000006
// #define  B300	0000007
// #define  B600	0000010
// #define  B1200	0000011
// #define  B1800	0000012
// #define  B2400	0000013
// #define  B4800	0000014
// #define  B9600	0000015
// #define  B19200	0000016
// #define  B38400	0000017

/* c_cflag bits.  */
// #define CSIZE	0000060
// #define   CS5	0000000
// #define   CS6	0000020
// #define   CS7	0000040
// #define   CS8	0000060
// #define CSTOPB	0000100
// #define CREAD	0000200
// #define PARENB	0000400
// #define PARODD	0001000
// #define HUPCL	0002000
// #define CLOCAL	0004000

/* c_lflag bits */
// #define ISIG	0000001   /* Enable signals.  */
#define ICANON	0000002   /* Canonical input (erase and kill processing).  */
// #define ECHO	0000010   /* Enable echo.  */
// #define ECHOE	0000020   /* Echo erase character as error-correcting backspace.  */
// #define ECHOK	0000040   /* Echo KILL.  */
// #define ECHONL	0000100   /* Echo NL.  */
// #define NOFLSH	0000200   /* Disable flush after interrupt or quit.  */
// #define TOSTOP	0000400   /* Send SIGTTOU for background output.  */

/* tcflow() and TCXONC use these */
// #define	TCOOFF		0
// #define	TCOON		1
// #define	TCIOFF		2
// #define	TCION		3

/* tcflush() and TCFLSH use these */
// #define	TCIFLUSH	0
// #define	TCOFLUSH	1
// #define	TCIOFLUSH	2

/* tcsetattr uses these.  */
// #define	TCSANOW		0
// #define	TCSADRAIN	1
// #define	TCSAFLUSH	2

#endif
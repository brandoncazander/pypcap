/* $Id$ */

#ifndef _WIN32
# include <sys/types.h>
# include <sys/ioctl.h>
# include <sys/time.h>
# include <fcntl.h>
# include <string.h>
# include <signal.h>
# include <unistd.h>
#endif

#include <pcap.h>
#ifdef HAVE_PCAP_INT_H
# include <pcap-int.h>
#endif
#include "pcap_ex.h"

#include "config.h"

/* XXX - hack around older Python versions */
#include "patchlevel.h"
#if PY_VERSION_HEX < 0x02030000
int    PyGILState_Ensure() { return (0); }
void   PyGILState_Release(int gil) { }
#endif

void
pcap_ex_immediate(pcap_t *pcap)
{
#ifdef BIOCIMMEDIATE
#ifdef HAVE_PCAP_FILE
	if (pcap_file(pcap) == NULL)
#else
        if (pcap->sf.rfile == NULL)
#endif
		ioctl(pcap_fileno(pcap), BIOCIMMEDIATE, 1);
#endif
}

char *
pcap_ex_name(char *name)
{
#ifdef _WIN32
	static char pcap_name[256];
        pcap_if_t *pifs, *cur, *prev, *next;
	char ebuf[128];
	int i, idx, max;

	if (strncmp(name, "eth", 3) != 0 ||
	    sscanf(name + 3, "%u", &idx) != 1 ||
	    pcap_findalldevs(&pifs, ebuf) == -1 || pifs == NULL) {
		return (name);
	}
	/* XXX - flip script like a dyslexic actor */
	for (prev = NULL, cur = pifs, max = 0; cur != NULL; max++) {
		next = cur->next;
		cur->next = prev;
		prev = cur;
		cur = next;
	}
	pifs = prev;
	for (cur = pifs, i = 0; i != idx && i < max; i++) {
		cur = cur->next;
	}
	if (i != max) {
		strncpy(pcap_name, cur->name, sizeof(pcap_name)-1);
		name = pcap_name;
	}
	pcap_freealldevs(pifs);
	return (name);
#else
	return (name);
#endif
}

int
pcap_ex_fileno(pcap_t *pcap)
{
#ifdef _WIN32
	/* XXX - how to handle savefiles? */
	return ((int)pcap_getevent(pcap));
#endif
#ifdef HAVE_PCAP_FILE
	FILE *f = pcap_file(pcap);
#else
	FILE *f = pcap->sf.rfile;
#endif
	if (f != NULL)
		return (fileno(f));
	return (pcap_fileno(pcap));
}

static int __pcap_ex_gotsig;

#ifdef _WIN32
static BOOL CALLBACK
__pcap_ex_ctrl(DWORD sig)
{
	__pcap_ex_gotsig = 1;
	return (TRUE);
}
#else
static void
__pcap_ex_signal(int sig)
{
	__pcap_ex_gotsig = 1;
}
#endif

/* XXX - hrr, this sux */
void
pcap_ex_setup(pcap_t *pcap)
{
#ifdef _WIN32
	SetConsoleCtrlHandler(__pcap_ex_ctrl, TRUE);
#else
	int fd, n;
	
	fd = pcap_fileno(pcap);
	n = fcntl(fd, F_GETFL, 0) | O_NONBLOCK;
	fcntl(fd, F_SETFL, n);

	signal(SIGINT, __pcap_ex_signal);
#endif
}

/* return codes: 1 = pkt, 0 = timeout, -1 = error, -2 = EOF */
int
pcap_ex_next(pcap_t *pcap, struct pcap_pkthdr **hdr, u_char **pkt)
{
#ifdef _WIN32
	if (__pcap_ex_gotsig) {
		__pcap_ex_gotsig = 0;
		return (-1);
	}
	return (pcap_next_ex(pcap, hdr, pkt));
#else
	static u_char *__pkt;
	static struct pcap_pkthdr __hdr;
	struct timeval tv = { 1, 0 };
	fd_set rfds;
	int fd, n;

	fd = pcap_fileno(pcap);
	for (;;) {
		if (__pcap_ex_gotsig) {
			__pcap_ex_gotsig = 0;
			return (-1);
		}
		if ((__pkt = (u_char *)pcap_next(pcap, &__hdr)) == NULL) {
#ifdef HAVE_PCAP_FILE
			if (pcap_file(pcap) != NULL)
#else
			if (pcap->sf.rfile != NULL)
#endif
				return (-2);
			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);
			n = select(fd + 1, &rfds, NULL, NULL, &tv);
			if (n <= 0)
				return (n);
		} else
			break;
	}
	*pkt = __pkt;
	*hdr = &__hdr;
	
	return (1);
#endif
}

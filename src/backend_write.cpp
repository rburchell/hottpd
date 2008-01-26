#include "inspircd.h"
#include "backend.h"
#include <sys/mman.h>

/* $Core: libhttpd_backend_write */

WriteBackend *WriteBackend::Instance = NULL;

int WriteBackend::ServeFile(int sockfd, int filefd, off_t &sent, off_t filesize)
{
	char *fdata = (char*) mmap(NULL, filesize, PROT_READ, MAP_SHARED, filefd, 0);
	if (fdata == MAP_FAILED)
	{
		ServerInstance->Log(DEBUG, "mmap to serve file failed: %s", strerror(errno));
		return -1;
	}
	
	ssize_t re = send(sockfd, fdata + sent, filesize - sent, MSG_DONTWAIT);

	munmap(fdata, filesize);

	if (re < 0)
	{
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
			return 0;
		
		ServerInstance->Log(DEBUG, "send to serve file to %d failed: %s", sockfd, strerror(errno));
		return -1;
	}
	
	sent += re;
	return re;
}


#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/param.h>
#include <dlfcn.h>
#include <syslog.h>

#include "opdi_platformfuncs.h"
#include "opdi_configspecs.h"
#include "opdi_constants.h"
#include "opdi_port.h"
#include "opdi_message.h"
#include "opdi_protocol.h"
#include "opdi_slave_protocol.h"
#include "opdi_config.h"

#include "LinuxOPDID.h"

// global connection mode (TCP or COM)
#define MODE_TCP 1
#define MODE_SERIAL 2

static int connection_mode = 0;
static char first_com_byte = 0;

static LinuxOPDID *linuxOPDID;

/** For TCP connections, receives a byte from the socket specified in info and places the result in byte.
*   For serial connections, reads a byte from the file handle specified in info and places the result in byte.
*   Blocks until data is available or the timeout expires.
*   If an error occurs returns an error code != 0.
*   If the connection has been gracefully closed, returns STATUS_DISCONNECTED.
*/
static uint8_t io_receive(void *info, uint8_t *byte, uint16_t timeout, uint8_t canSend) {
	char c;
	int result;
	long ticks = opdi_get_time_ms();

	while (1) {
		// call work function
		uint8_t waitResult = Opdi->waiting(canSend);
		if (waitResult != OPDI_STATUS_OK)
			return waitResult;

		if (connection_mode == MODE_TCP) {

			int newsockfd = (long)info;

			// try to read data
			result = read(newsockfd, &c, 1);
			if (result < 0) {
				// timed out?
				if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
					// possible timeout
					// "real" timeout condition
					if (opdi_get_time_ms() - ticks >= timeout)
						return OPDI_TIMEOUT;
				} else
				// perhaps Ctrl+C
				if (errno == EINTR) {
					Opdi->shutdown();
				}
				else {
					printf("Error: %d\n", errno);
					// other error condition
					return OPDI_NETWORK_ERROR;
				}
			}
			else
			// connection closed?
			if (result == 0)
				// dirty disconnect
				return OPDI_NETWORK_ERROR;
			else
				// a byte has been received
//				printf("%i", c);
				break;
		}
		else
		if (connection_mode == MODE_SERIAL) {
			int fd = (long)info;
			char inputData;
			int bytesRead;

			// first byte of connection remembered?
			if (first_com_byte != 0) {
				c = first_com_byte;
				first_com_byte = 0;
				break;
			}

			if ((bytesRead = read(fd, &inputData, 1)) >= 0) {
				if (bytesRead == 1) {
					// a byte has been received
					c = inputData;
					break;
				}
				else {
					// ran into timeout
					// "real" timeout condition
					if (opdi_get_time_ms() - ticks >= timeout)
						return OPDI_TIMEOUT;
				}
			}
			else {
				// device error
				return OPDI_DEVICE_ERROR;
			}
		}
	}

	*byte = (uint8_t)c;

	return OPDI_STATUS_OK;
}

/** For TCP connections, sends count bytes to the socket specified in info.
*   For serial connections, writes count bytes to the file handle specified in info.
*   If an error occurs returns an error code != 0. */
static uint8_t io_send(void *info, uint8_t *bytes, uint16_t count) {
	char *c = (char *)bytes;

	if (connection_mode == MODE_TCP) {

		int newsockfd = (long)info;
sendloop:
		int result = send(newsockfd, c, count, MSG_DONTWAIT);
		if (result < 0) {
			// send buffer full?
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
				// wait for some time
				usleep(10000);
				goto sendloop;
			} else
			// perhaps Ctrl+C
			if (errno == EINTR) {
				Opdi->shutdown();
			}
			else {
			linuxOPDID->logError(std::string("Socket send failed: " ) + strerror(errno));
			return OPDI_DEVICE_ERROR;
			}
		} else
		// incomplete send?
		if (result < count) {
			// advance pointer, decrease remaining count and send again
			bytes += result;
			count -= result;
			goto sendloop;
		}
		// send ok
	}
	else
	if (connection_mode == MODE_SERIAL) {
		int fd = (long)info;

		if (write(fd, c, count) != count) {
			linuxOPDID->logError(std::string("Serial write failed: " ) + strerror(errno));
			return OPDI_DEVICE_ERROR;
		}
	}

	return OPDI_STATUS_OK;
}

LinuxOPDID::LinuxOPDID(void)
{
	this->framesPerSecond = 0;
}

LinuxOPDID::~LinuxOPDID(void)
{
}

void LinuxOPDID::print(const char *text) {
	// text is treated as UTF8.
	std::cout << text;
}

void LinuxOPDID::println(const char *text) {
	// text is treated as UTF8.
	std::cout << text << std::endl;
}

void LinuxOPDID::printe(const char *text) {
	// text is treated as UTF8.
	std::cerr << text;
}

void LinuxOPDID::printlne(const char *text) {
	// text is treated as UTF8.
	std::cerr << text << std::endl;
}

/** This method handles an incoming TCP connection. It blocks until the connection is closed.
*/
int LinuxOPDID::HandleTCPConnection(int csock) {
	opdi_Message message;
	uint8_t result;

	connection_mode = MODE_TCP;

	struct timeval aTimeout;
	aTimeout.tv_sec = 0;
	aTimeout.tv_usec = 1000;		// one ms timeout

	// set timeouts on socket
	if (setsockopt (csock, SOL_SOCKET, SO_RCVTIMEO, (char *)&aTimeout, sizeof(aTimeout)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}
	if (setsockopt (csock, SOL_SOCKET, SO_SNDTIMEO, (char *)&aTimeout, sizeof(aTimeout)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}

	// set linger on socket
	struct linger sLinger;
	sLinger.l_onoff = 1;
	sLinger.l_linger = 1;
	if (setsockopt (csock, SOL_SOCKET, SO_LINGER, (char *)&sLinger, sizeof(sLinger)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}

	// set TCP_NODELAY
	int flag = 1;
	if (setsockopt(csock, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}

	// info value is the socket handle
	result = opdi_message_setup(&io_receive, &io_send, (void *)(long)csock);
	if (result != 0)
		return result;

	result = opdi_get_message(&message, OPDI_CANNOT_SEND);
	if (result != 0)
		return result;

	last_activity = opdi_get_time_ms();

	// initiate handshake
	result = opdi_slave_start(&message, NULL, &protocol_callback);

	return result;
}

int LinuxOPDID::setupTCP(std::string interface_, int port) {

	// store instance reference
	linuxOPDID = this;

	// adapted from: http://www.linuxhowtos.org/C_C++/socket.htm

	int err = 0;

	int sockfd, newsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	// create socket (non-blocking)
	sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | TCP_NODELAY, 0);
	if (sockfd < 0) {
		throw Poco::ApplicationException("ERROR opening socket", errno);
	}

	// set TCP_NODELAY
	int flag = 1;
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &flag, sizeof(int)) < 0) {
		this->log("setsockopt failed");
		return OPDI_DEVICE_ERROR;
	}

	// prepare address
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	// bind to specified port
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		throw Poco::ApplicationException("ERROR on binding", errno);
	}

	// listen for an incoming connection
	listen(sockfd, 1);

	int sleepRemainderBase = 1000;
	int sleepRemainderAdjustCount = 0;

	while (true) {
        	if (Opdi->logVerbosity != QUIET)
			this->log(std::string("Listening for a connection on TCP port ") + this->to_string(port));

		while (true) {

			clilen = sizeof(cli_addr);
			newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
			if (newsockfd < 0) {
				if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
					// measure processing time
					struct timeval tv;
					gettimeofday(&tv, NULL);
					long proctime = 1000000 * tv.tv_sec + tv.tv_usec;

					// not yet connected; process housekeeping about once a millisecond
					uint8_t waitResult = this->waiting(false);
					if (waitResult != OPDI_STATUS_OK)
						return waitResult;

					gettimeofday(&tv, NULL);
					long elapsed = 1000000 * tv.tv_sec + tv.tv_usec - proctime;

					// sleep for remainder of the millisecond
					//  usleep has some overhead that might be different on different systems
					// calculate sleep remainder base to approximate the specified target fps
					if (sleepRemainderAdjustCount > 100) {
						sleepRemainderAdjustCount = 0;
						if (this->framesPerSecond > 0) {
							if (this->framesPerSecond > this->targetFramesPerSecond)
								sleepRemainderBase++;
							else if (this->framesPerSecond < this->targetFramesPerSecond)
								sleepRemainderBase--;
						}
					} else
						sleepRemainderAdjustCount++;

					if (elapsed < sleepRemainderBase) {
						usleep(sleepRemainderBase - elapsed);
					}
				} else
					this->log(std::string("Error accepting connection: ") + this->to_string(errno));
			} else {

				if (Opdi->logVerbosity != QUIET)
					this->log((std::string("Connection attempt from ") + std::string(inet_ntoa(cli_addr.sin_addr))).c_str());

				err = HandleTCPConnection(newsockfd);

//				shutdown(newsockfd, SHUT_WR);

				// TODO maybe there's a better way to ensure that data is sent?
//				usleep(500);

				close(newsockfd);

				// shutdown requested?
				if (this->shutdownRequested)
					return OPDI_SHUTDOWN;

				if (Opdi->logVerbosity != QUIET)
					this->log(std::string("Result: ") + this->getOPDIResult(err));

				break;
			}
		}
        }

	return 0;
}

IOPDIDPlugin *LinuxOPDID::getPlugin(std::string driver) {

	this->warnIfPluginMoreRecent(driver);

	void *hndl = dlopen(driver.c_str(), RTLD_NOW);
	if (hndl == NULL){
		throw Poco::FileException("Could not load the plugin library", dlerror());
	}

	dlerror();
	// getPluginInstance can't be declared as void* because this emits a -pedantic warning:
	// "ISO C++ forbids casting between pointer-to-function and pointer-to-object"
	// This trick is described here: https://github.com/christopherpoole/cppplugin/wiki/Plugins-in-CPP%3A-Dynamically-Linking-Shared-Objects
	IOPDIDPlugin* (*getPluginInstance)(int, int, int);
	*(void **)(&getPluginInstance) = dlsym(hndl, "GetOPDIDPluginInstance");

	char *lasterror = dlerror();
	if (lasterror != NULL) {
		dlclose(hndl);
		throw Poco::ApplicationException("Invalid plugin library; could not locate function 'GetOPDIDPluginInstance' in " + driver, lasterror);
	}

	// call the library function to get the plugin instance
	return ((IOPDIDPlugin *(*)(int, int, int))(getPluginInstance))(this->majorVersion, this->minorVersion, this->patchVersion);
}

#pragma once

// need to guard against security check warnings
#define _SCL_SECURE_NO_WARNINGS	1

#include <sstream>
#include <fstream>
#include <list>

// serial port library
#include "ctb-0.16/ctb.h"

#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/TimedNotificationQueue.h"

#include "opdi_constants.h"

#include "OPDID_PortFunctions.h"

///////////////////////////////////////////////////////////////////////////////
// Logic Port
///////////////////////////////////////////////////////////////////////////////

/** A LogicPort implements logic functions for digital ports. It supports
* the following operations:
* - OR (default): The line is High if at least one of its inputs is High
* - AND: The line is High if all of its inputs are High
* - XOR: The line is High if an odd number of its inputs is High
* - ATLEAST (n): The line is High if at least n inputs are High
* - ATMOST (n): The line is High if at most n inputs are High
* - EXACT (n): The line is High if exactly n inputs are High
* Additionally you can specify whether the output should be negated.
* The LogicPort requires at least one digital port as input. The output
* can optionally be distributed to an arbitrary number of digital ports.
* Processing occurs in the OPDI waiting event loop, i. e. about once a ms.
* All input ports' state is queried. If the logic function results in a change
* of this port's state the new state is set on the output ports. This means that
* there is no unnecessary continuous state propagation.
* If the port is not hidden it will perform a self-refresh when the state changes.
* Non-hidden output ports whose state is changed will be refreshed.
* You can also specify inverted output ports who will be updated with the negated
* state of this port.
*/
class OPDID_LogicPort : public OPDI_DigitalPort, protected OPDID_PortFunctions {
protected:
	enum LogicFunction {
		UNKNOWN,
		OR,
		AND,
		XOR,
		ATLEAST,
		ATMOST,
		EXACT
	};

	LogicFunction function;
	size_t funcN;
	bool negate;
	std::string inputPortStr;
	std::string outputPortStr;
	std::string inverseOutputPortStr;
	DigitalPortList inputPorts;
	DigitalPortList outputPorts;
	DigitalPortList inverseOutputPorts;

	virtual uint8_t doWork(uint8_t canSend);

public:
	OPDID_LogicPort(AbstractOPDID *opdid, const char *id);

	virtual ~OPDID_LogicPort();

	virtual void configure(Poco::Util::AbstractConfiguration *config);

	virtual void setDirCaps(const char *dirCaps) override;

	virtual void setMode(uint8_t mode) override;

	virtual void setLine(uint8_t line) override;

	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Pulse Port
///////////////////////////////////////////////////////////////////////////////

/** A pulse port generates a digital pulse with a defined period (measured
* in milliseconds) and a duty cycle in percent. The period and duty cycle
* can optionally be set by analog ports. The period is in this case
* calculated as the percentage of Period. The pulse is active if the line
* of this port is set to High. If enable digital ports are specified the
* pulse is also being generated if at least one of the enable ports is High.
* The output can be normal or inverted. There are two lists of output digital
* ports which receive the normal or inverted output respectively.
*/
class OPDID_PulsePort : public OPDI_DigitalPort, protected OPDID_PortFunctions {
protected:
	bool negate;
	int32_t period;
	double dutyCycle;
	int8_t disabledState;
	std::string periodPortStr;
	std::string dutyCyclePortStr;
	std::string enablePortStr;
	std::string outputPortStr;
	std::string inverseOutputPortStr;
	typedef std::vector<OPDI_DigitalPort *> PortList;
	DigitalPortList enablePorts;
	DigitalPortList outputPorts;
	DigitalPortList inverseOutputPorts;
	OPDI_AnalogPort *periodPort;
	OPDI_AnalogPort *dutyCyclePort;

	// state
	uint8_t pulseState;
	uint64_t lastStateChangeTime;

	virtual uint8_t doWork(uint8_t canSend);

public:
	OPDID_PulsePort(AbstractOPDID *opdid, const char *id);

	virtual ~OPDID_PulsePort();

	virtual void configure(Poco::Util::AbstractConfiguration *config);

	virtual void setDirCaps(const char *dirCaps) override;

	virtual void setMode(uint8_t mode) override;

	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Selector Port
///////////////////////////////////////////////////////////////////////////////

/** A SelectorPort is a DigitalPort that is High when the specified select port
*   is in the specified position and Low otherwise. If set to High it will switch
*   the select port to the specified position. If set to Low, it will do nothing.
*/
class OPDID_SelectorPort : public OPDI_DigitalPort, protected OPDID_PortFunctions {
protected:
	std::string selectPortStr;
	OPDI_SelectPort *selectPort;
	uint16_t position;

	virtual uint8_t doWork(uint8_t canSend);

public:
	OPDID_SelectorPort(AbstractOPDID *opdid, const char *id);

	virtual ~OPDID_SelectorPort();

	virtual void configure(Poco::Util::AbstractConfiguration *config);

	virtual void setDirCaps(const char *dirCaps);

	virtual void setMode(uint8_t mode);

	virtual void setLine(uint8_t line);

	virtual void prepare();
};

///////////////////////////////////////////////////////////////////////////////
// Error Detector Port
///////////////////////////////////////////////////////////////////////////////

/** An ErrorDetectorPort is a DigitalPort whose state is determined by one or more 
*   specified ports. If any of these ports will have an error, i. e. their hasError()
*   method returns true, the state of this port will be High and Low otherwise.
*   The logic level can be negated.
*/
class OPDID_ErrorDetectorPort : public OPDI_DigitalPort, protected OPDID_PortFunctions {
protected:
	bool negate;
	std::string inputPortStr;
	OPDI::PortList inputPorts;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	OPDID_ErrorDetectorPort(AbstractOPDID *opdid, const char *id);

	virtual ~OPDID_ErrorDetectorPort();

	virtual void configure(Poco::Util::AbstractConfiguration *config);

	virtual void setDirCaps(const char *dirCaps) override;

	virtual void setMode(uint8_t mode) override;

	virtual void prepare() override;
};

///////////////////////////////////////////////////////////////////////////////
// Serial Streaming Port
///////////////////////////////////////////////////////////////////////////////

/** Defines a serial streaming port that supports streaming from and to a serial port device.
 */
class OPDID_SerialStreamingPort : public OPDI_StreamingPort, protected OPDID_PortFunctions {
friend class OPDI;

protected:
	// a serial streaming port may pass the bytes through or return them in the doWork method (loopback)
	enum Mode {
		PASS_THROUGH,
		LOOPBACK
	};

	Mode mode;

	ctb::IOBase* device;
	ctb::SerialPort* serialPort;

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	OPDID_SerialStreamingPort(AbstractOPDID *opdid, const char *id);

	virtual ~OPDID_SerialStreamingPort();

	virtual void configure(Poco::Util::AbstractConfiguration *config);

	virtual int write(char *bytes, size_t length) override;

	virtual int available(size_t count) override;

	virtual int read(char *result) override;

	virtual bool hasError(void) override;
};

///////////////////////////////////////////////////////////////////////////////
// Logger Streaming Port
///////////////////////////////////////////////////////////////////////////////

/** Defines a streaming port that can log port states and optionally write them to a log file.
 */
class OPDID_LoggerPort : public OPDI_StreamingPort, protected OPDID_PortFunctions {
friend class OPDI;

protected:
	enum Format {
		CSV
	};

	uint32_t logPeriod;
	Format format;
	std::string separator;
	std::string portsToLogStr;
	OPDI::PortList portsToLog;

	uint64_t lastEntryTime;
	std::ofstream outFile;

	std::string getPortStateStr(OPDI_Port* port);

	virtual uint8_t doWork(uint8_t canSend) override;

public:
	OPDID_LoggerPort(AbstractOPDID *opdid, const char *id);

	virtual ~OPDID_LoggerPort();

	virtual void configure(Poco::Util::AbstractConfiguration *config);

	virtual void prepare() override;

	virtual int write(char *bytes, size_t length) override;

	virtual int available(size_t count) override;

	virtual int read(char *result) override;

	virtual bool hasError(void) override;
};

///////////////////////////////////////////////////////////////////////////////
// Fader Port
///////////////////////////////////////////////////////////////////////////////

/** A FaderPort can fade AnalogPorts in or out. You can set a left and right value
* as well as a duration time in milliseconds. If the FaderPort is inactive (line = Low),
* it will not act on the output ports. If it is set to High it will start at the
* left value and count to the right value within the specified time. The values have 
* to be specified as percentages. Once the duration is up the port sets itself to 
* inactive (line = Low).
*/
class OPDID_FaderPort : public OPDI_DigitalPort, protected OPDID_PortFunctions {
protected:
	enum FaderMode {
		LINEAR,
		EXPONENTIAL
	};

	FaderMode mode;
	double left;
	double right;
	int durationMs;
	double expA;
	double expB;
	double expMax;
	bool invert;

	std::string outputPortStr;
	PortList outputPorts;

	Poco::Timestamp startTime;
	double lastValue;

	virtual uint8_t doWork(uint8_t canSend);

public:
	OPDID_FaderPort(AbstractOPDID *opdid, const char *id);

	virtual ~OPDID_FaderPort();

	virtual void configure(Poco::Util::AbstractConfiguration *config);

	virtual void setDirCaps(const char *dirCaps) override;

	virtual void setMode(uint8_t mode) override;

	virtual void setLine(uint8_t line) override;

	virtual void prepare() override;
};

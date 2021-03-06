#pragma once

#include <sstream>
#include <list>

#include "Poco/Mutex.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/Logger.h"
#include "Poco/Stopwatch.h"
#include "Poco/BasicEvent.h"
#include "Poco/Delegate.h"

#include "OPDIDConfigurationFile.h"

#include "opdi_configspecs.h"
#include "OPDI.h"

// protocol callback function for the OPDI slave implementation
extern void protocol_callback(uint8_t state);

namespace opdid {
	class AbstractOPDID;
}

/** The abstract plugin interface. */
struct IOPDIDPlugin {
	// config is the parent configuration. Implementations should use createView to get the node configuration.
	virtual void setupPlugin(opdid::AbstractOPDID* abstractOPDID, const std::string& nodeName, Poco::Util::AbstractConfiguration* config) = 0;

	// virtual destructor (called when the plugin is deleted)
	virtual ~IOPDIDPlugin() {}
};

namespace opdid {

#define OPDID_CONFIG_FILE_SETTING	"__OPDID_CONFIG_FILE_PATH"

#define OPDID_MAJOR_VERSION		0
#define OPDID_MINOR_VERSION		1
#define OPDID_PATCH_VERSION		0

/** The listener interface for plugin registrations. */
struct IOPDIDConnectionListener {
	/** Is called when a master successfully completed the handshake. */
	virtual void masterConnected(void) = 0;

	/** Is called when a master has disconnected. */
	virtual void masterDisconnected(void) = 0;
};

/** The abstract base class for OPDID implementations. */
class AbstractOPDID: public opdi::OPDI {
protected:
	int majorVersion;
	int minorVersion;
	int patchVersion;

	bool allowHiddenPorts;

	// user and password for master authentication (set from the configuration)
	std::string loginUser;
	std::string loginPassword;

	std::string deviceInfo;

	// environment variables for config file substitution (keys prefixed with $)
	std::map<std::string, std::string> environment;

	Poco::Mutex mutex;
	Poco::Logger* logger;

	typedef std::list<IOPDIDConnectionListener*> ConnectionListenerList;
	ConnectionListenerList connectionListeners;

	typedef std::map<uint8_t, std::string> OPDICodeTexts;
	OPDICodeTexts opdiCodeTexts;

	typedef std::map<std::string, std::string> LockedResources;
	LockedResources lockedResources;

	typedef std::list<Poco::SharedPtr<IOPDIDPlugin>> PluginList;
	PluginList pluginList;

	// internal status monitoring variables
	static const int maxSecondStats = 1100;
	uint64_t* monSecondStats;				// doWork performance statistics buffer
	int monSecondPos;						// current position in buffer
	Poco::Stopwatch idleStopwatch;			// measures time until waiting() is called again
	uint64_t totalMicroseconds;				// total time (doWork + idle)
	int waitingCallsPerSecond;				// number of calls to waiting()
	double framesPerSecond;					// average number of doWork iterations ("frames") processed per second
	int targetFramesPerSecond;				// target number of doWork iterations per second

	std::string heartbeatFile;

	virtual uint8_t idleTimeoutReached(void) override;

	virtual Poco::Util::AbstractConfiguration* readConfiguration(const std::string& fileName, const std::map<std::string, std::string>& parameters);

	/** Outputs a log message with a timestamp. */
	virtual void log(const std::string& text);

	virtual void logErr(const std::string& message);

	virtual void logWarn(const std::string& message);
public:

	std::string timestampFormat;

	Poco::BasicEvent<void> allPortsRefreshed;
	Poco::BasicEvent<opdi::Port*> portRefreshed;

	// configuration file for port state persistence
	std::string persistentConfigFile;
	Poco::Util::PropertyFileConfiguration* persistentConfig;

	AbstractOPDID(void);

	virtual ~AbstractOPDID(void);

	virtual void protocolCallback(uint8_t protState);

	virtual void connected(void);

	virtual void disconnected(void);

	virtual void addConnectionListener(IOPDIDConnectionListener* listener);

	virtual void removeConnectionListener(IOPDIDConnectionListener* listener);

	/** Outputs a hello message. */
	virtual void sayHello(void);

	virtual void showHelp(void);

	/** Returns the current user ID or name. */
	virtual std::string getCurrentUser(void) = 0;

	/** Modify the current process credentials to a less privileged user. */
	virtual void switchToUser(std::string newUser) = 0;

	virtual std::string getTimestampStr(void);

	virtual std::string getOPDIResult(uint8_t code);

	/** Returns the key's value from the configuration or the default value, if it is missing. If missing and isRequired is true, throws an exception. */
	virtual std::string getConfigString(Poco::Util::AbstractConfiguration* config, const std::string &section, const std::string &key, const std::string &defaultValue, const bool isRequired);

	/** Outputs the specified text to an implementation-dependent output with an appended line break. */
	virtual void println(const char* text) = 0;

	/** Outputs the specified text to an implementation-dependent output with an appended line break. */
	virtual void println(const std::string& text);

	/** Outputs the specified error text to an implementation-dependent error output with an appended line break. */
	virtual void printlne(const char* text) = 0;

	/** Outputs the specified error text to an implementation-dependent error output with an appended line break. */
	virtual void printlne(const std::string& text);

	/** Starts processing the supplied arguments. */
	virtual int startup(const std::vector<std::string>& args, const std::map<std::string, std::string>& environment);

	/** Attempts to lock the resource with the specified ID. The resource can be anything, a pin number, a file name or whatever.
	 *  When the same resource is attempted to be locked twice this method throws an exception.
	 *  Use this mechanism to avoid resource conflicts. */
	virtual void lockResource(const std::string& resourceID, const std::string& lockerID);

	virtual opdi::LogVerbosity getConfigLogVerbosity(Poco::Util::AbstractConfiguration* config, opdi::LogVerbosity defaultVerbosity);

	/** Returns the configuration that should be used for querying a port's state. This is the baseConfig if no
	 *  persistent configuration has been specified, or a layered configuration otherwise.
	 *	This configuration must be freed after use. The view name is optional. */
	virtual Poco::Util::AbstractConfiguration* getConfigForState(Poco::Util::AbstractConfiguration* baseConfig, const std::string& viewName);

	virtual void setGeneralConfiguration(Poco::Util::AbstractConfiguration* general);

	virtual void configureEncryption(Poco::Util::AbstractConfiguration* config);

	virtual void configureAuthentication(Poco::Util::AbstractConfiguration* config);

	/** Reads common properties from the configuration and configures the port group. */
	virtual void configureGroup(Poco::Util::AbstractConfiguration* groupConfig, opdi::PortGroup* group, int defaultFlags);

	virtual void setupGroup(Poco::Util::AbstractConfiguration* groupConfig, const std::string& group);

	virtual std::string resolveRelativePath(Poco::Util::AbstractConfiguration* config, const std::string& source, const std::string& path, const std::string defaultValue);

	virtual void setupInclude(Poco::Util::AbstractConfiguration* groupConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& node);

	/** Reads common properties from the configuration and configures the port. */
	virtual void configurePort(Poco::Util::AbstractConfiguration* portConfig, opdi::Port* port, int defaultFlags);

	/** Reads special properties from the configuration and configures the digital port. */
	virtual void configureDigitalPort(Poco::Util::AbstractConfiguration* portConfig, opdi::DigitalPort* port, bool stateOnly = false);

	virtual void setupEmulatedDigitalPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	/** Reads special properties from the configuration and configures the analog port. */
	virtual void configureAnalogPort(Poco::Util::AbstractConfiguration* portConfig, opdi::AnalogPort* port, bool stateOnly = false);

	virtual void setupEmulatedAnalogPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	/** Reads special properties from the configuration and configures the select port. */
	virtual void configureSelectPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, opdi::SelectPort* port, bool stateOnly = false);

	virtual void setupEmulatedSelectPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port);

	/** Reads special properties from the configuration and configures the dial port. */
	virtual void configureDialPort(Poco::Util::AbstractConfiguration* portConfig, opdi::DialPort* port, bool stateOnly = false);

	virtual void setupEmulatedDialPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	/** Reads special properties from the configuration and configures the streaming port. */
	virtual void configureStreamingPort(Poco::Util::AbstractConfiguration* portConfig, opdi::StreamingPort* port);

	virtual void setupSerialStreamingPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	virtual void setupLoggerPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	virtual void setupLogicPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	virtual void setupPulsePort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	virtual void setupSelectorPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

#ifdef OPDID_USE_EXPRTK
	virtual void setupExpressionPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);
#endif	// def OPDID_USE_EXPRTK

	virtual void setupTimerPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port);

	virtual void setupErrorDetectorPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	virtual void setupFaderPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	virtual void setupExecPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	virtual void setupSceneSelectPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port);

	virtual void setupFilePort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port);

	virtual void setupAggregatorPort(Poco::Util::AbstractConfiguration* portConfig, Poco::Util::AbstractConfiguration* parentConfig, const std::string& port);

	virtual void setupTriggerPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	virtual void setupCounterPort(Poco::Util::AbstractConfiguration* portConfig, const std::string& port);

	/** Configures the specified node. */
	virtual void setupNode(Poco::Util::AbstractConfiguration* config, const std::string& node);

	/** Starts enumerating the nodes of the Root section and configures the nodes. */
	virtual void setupRoot(Poco::Util::AbstractConfiguration* config);

	/** Sets up the connection from the specified configuration. */
	virtual int setupConnection(Poco::Util::AbstractConfiguration* config, bool testMode);

	/** Sets up a TCP listener and listens for incoming requests. This method does not return unless the program should exit. */
	virtual int setupTCP(std::string interface_, int port) = 0;

	/** Checks whether the supplied file is more recent than the current binary and logs a warning if yes. */
	virtual void warnIfPluginMoreRecent(const std::string& driver);

	/** Returns a pointer to the plugin object instance specified by the given driver. */
	virtual IOPDIDPlugin* getPlugin(std::string driver) = 0;

	virtual uint8_t waiting(uint8_t canSend) override;

	/* Authenticate comparing the login data with the configuration login data. */
	virtual uint8_t setPassword(const std::string& password) override;

	virtual std::string getExtendedDeviceInfo(void) override;

	/** This implementation also logs the refreshed ports. */
	virtual uint8_t refresh(opdi::Port** ports) override;

	virtual void savePersistentConfig();

	/** Implements a persistence mechanism for port states. */
	virtual void persist(opdi::Port* port) override;

	/** Returns a string representing the port state; empty in case of errors. */
	virtual std::string getPortStateStr(opdi::Port* port) const;

	virtual std::string getDeviceInfo(void);

	virtual void getEnvironment(std::map<std::string, std::string>& mapToFill);
};

}		// namespace opdid

/** Define external singleton instance */
extern opdid::AbstractOPDID* Opdi;


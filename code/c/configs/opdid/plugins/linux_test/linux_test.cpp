
// Test plugin for OPDID (Windows version)

#include "opdi_constants.h"

#include "LinuxOPDID.h"

class DigitalTestPort : public OPDI_DigitalPort {
public:
	DigitalTestPort();
	virtual uint8_t setLine(uint8_t line) override;
};

DigitalTestPort::DigitalTestPort() : OPDI_DigitalPort("PluginPort", "Windows Test Plugin Port", OPDI_PORTDIRCAP_OUTPUT, 0) {}

uint8_t DigitalTestPort::setLine(uint8_t line) {
	OPDI_DigitalPort::setLine(line);

	AbstractOPDID *opdid = (AbstractOPDID *)this->opdi;
	if (line == 0) {
		opdid->println("DigitalTestPort line set to Low");
	} else {
		opdid->println("DigitalTestPort line set to High");
	}

	return OPDI_STATUS_OK;
}

class LinuxTestOPDIDPlugin : public IOPDIDPlugin, public IOPDIDConnectionListener {

protected:
	AbstractOPDID *opdid;

public:
	virtual void setupPlugin(AbstractOPDID *abstractOPDID, std::string node, Poco::Util::AbstractConfiguration *nodeConfig);

	virtual void masterConnected(void) override;
	virtual void masterDisconnected(void) override;
};


void LinuxTestOPDIDPlugin::setupPlugin(AbstractOPDID *abstractOPDID, std::string node, Poco::Util::AbstractConfiguration *nodeConfig) {
	this->opdid = abstractOPDID;

	// get port type
	std::string portType = nodeConfig->getString("Type", "");

	if (portType == "DigitalPort") {
		// add emulated test port
		DigitalTestPort *port = new DigitalTestPort();
		abstractOPDID->configureDigitalPort(nodeConfig, port);
		abstractOPDID->addPort(port);
	} else
		throw Poco::DataException("This plugin supports only node type 'DigitalPort'", portType);

	if (this->opdid->logVerbosity == AbstractOPDID::VERBOSE)
		this->opdid->println("LinuxTestOPDIDPlugin setup completed successfully as node " + node);
}

void LinuxTestOPDIDPlugin::masterConnected() {
	if (this->opdid->logVerbosity != AbstractOPDID::QUIET)
		this->opdid->log("Test plugin: master connected");
}

void LinuxTestOPDIDPlugin::masterDisconnected() {
	if (this->opdid->logVerbosity != AbstractOPDID::QUIET)
		this->opdid->log("Test plugin: master disconnected");
}

extern "C" IOPDIDPlugin* GetOPDIDPluginInstance() {
	// return a new instance of this plugin
	return new LinuxTestOPDIDPlugin();
}
// Local-HyperHDR includes
#include "LedDeviceNanoleaf.h"
#include <utils/QStringUtils.h>
#include <ssdp/SSDPDiscover.h>

// Qt includes
#include <QEventLoop>
#include <QNetworkReply>
#include <QtEndian>

//std includes
#include <sstream>
#include <iomanip>

// Constants
namespace {
const bool verbose = false;
const bool verbose3 = false;

// Configuration settings
const char CONFIG_ADDRESS[] = "host";
//const char CONFIG_PORT[] = "port";
const char CONFIG_AUTH_TOKEN[] = "token";

const char CONFIG_PANEL_ORDER_TOP_DOWN[] = "panelOrderTopDown";
const char CONFIG_PANEL_ORDER_LEFT_RIGHT[] = "panelOrderLeftRight";
const char CONFIG_PANEL_START_POS[] = "panelStartPos";

// Panel configuration settings
const char PANEL_LAYOUT[] = "layout";
const char PANEL_NUM[] = "numPanels";
const char PANEL_ID[] = "panelId";
const char PANEL_POSITIONDATA[] = "positionData";
const char PANEL_SHAPE_TYPE[] = "shapeType";
//const char PANEL_ORIENTATION[] = "0";
const char PANEL_POS_X[] = "x";
const char PANEL_POS_Y[] = "y";

// List of State Information
const char STATE_ON[] = "on";
const char STATE_ONOFF_VALUE[] = "value";
const char STATE_VALUE_TRUE[] = "true";
const char STATE_VALUE_FALSE[] = "false";

// Device Data elements
const char DEV_DATA_NAME[] = "name";
const char DEV_DATA_MODEL[] = "model";
const char DEV_DATA_MANUFACTURER[] = "manufacturer";
const char DEV_DATA_FIRMWAREVERSION[] = "firmwareVersion";

// Nanoleaf Stream Control elements
//const char STREAM_CONTROL_IP[] = "streamControlIpAddr";
const char STREAM_CONTROL_PORT[] = "streamControlPort";
//const char STREAM_CONTROL_PROTOCOL[] = "streamControlProtocol";
const quint16 STREAM_CONTROL_DEFAULT_PORT = 60222; //Fixed port for Canvas;

// Nanoleaf OpenAPI URLs
const int API_DEFAULT_PORT = 16021;
const char API_BASE_PATH[] = "/api/v1/%1/";
const char API_ROOT[] = "";
//const char API_EXT_MODE_STRING_V1[] = "{\"write\" : {\"command\" : \"display\", \"animType\" : \"extControl\"}}";
const char API_EXT_MODE_STRING_V2[] = "{\"write\" : {\"command\" : \"display\", \"animType\" : \"extControl\", \"extControlVersion\" : \"v2\"}}";
const char API_STATE[] = "state";
const char API_PANELLAYOUT[] = "panelLayout";
const char API_EFFECT[] = "effects";

//Nanoleaf Control data stream
const int STREAM_FRAME_PANEL_NUM_SIZE = 2;
const int STREAM_FRAME_PANEL_INFO_SIZE = 8;

// Nanoleaf ssdp services
const char SSDP_ID[] = "ssdp:all";
const char SSDP_FILTER_HEADER[] = "ST";
const char SSDP_CANVAS[] = "nanoleaf:nl29";
const char SSDP_LIGHTPANELS[] = "nanoleaf_aurora:light";
} //End of constants

// Nanoleaf Panel Shapetypes
enum SHAPETYPES {
	TRIANGLE,
	RHYTM,
	SQUARE,
	CONTROL_SQUARE_PRIMARY,
	CONTROL_SQUARE_PASSIVE,
	POWER_SUPPLY,
	};

// Nanoleaf external control versions
enum EXTCONTROLVERSIONS {
	EXTCTRLVER_V1 = 1,
	EXTCTRLVER_V2
};

LedDeviceNanoleaf::LedDeviceNanoleaf(const QJsonObject& deviceConfig)
	: ProviderUdp(deviceConfig)
	  , _restApi(nullptr)
	  , _apiPort(API_DEFAULT_PORT)
	  , _topDown(true)
	  , _leftRight(true)
	  , _startPos(0)
	  , _endPos(0)
	  , _extControlVersion(EXTCTRLVER_V2),
	  _panelLedCount(0)
{
}

LedDevice* LedDeviceNanoleaf::construct(const QJsonObject& deviceConfig)
{
	return new LedDeviceNanoleaf(deviceConfig);
}

LedDeviceNanoleaf::~LedDeviceNanoleaf()
{
	delete _restApi;
	_restApi = nullptr;
}

bool LedDeviceNanoleaf::init(const QJsonObject& deviceConfig)
{
	// Overwrite non supported/required features
	setLatchTime(0);
	setRewriteTime(0);

	if (deviceConfig["rewriteTime"].toInt(0) > 0)
	{
		Info(_log, "Device Nanoleaf does not require rewrites. Refresh time is ignored.");
	}

	DebugIf(verbose, _log, "deviceConfig: [%s]", QString(QJsonDocument(_devConfig).toJson(QJsonDocument::Compact)).toUtf8().constData());

	bool isInitOK = false;

	if (LedDevice::init(deviceConfig))
	{
		int configuredLedCount = this->getLedCount();
		Debug(_log, "DeviceType   : %s", QSTRING_CSTR(this->getActiveDeviceType()));
		Debug(_log, "LedCount     : %d", configuredLedCount);
		Debug(_log, "ColorOrder   : %s", QSTRING_CSTR(this->getColorOrder()));
		Debug(_log, "RewriteTime  : %d", this->getRewriteTime());
		Debug(_log, "LatchTime    : %d", this->getLatchTime());

		// Read panel organisation configuration
		if (deviceConfig[CONFIG_PANEL_ORDER_TOP_DOWN].isString())
		{
			_topDown = deviceConfig[CONFIG_PANEL_ORDER_TOP_DOWN].toString().toInt() == 0;
		}
		else
		{
			_topDown = deviceConfig[CONFIG_PANEL_ORDER_TOP_DOWN].toInt() == 0;
		}

		if (deviceConfig[CONFIG_PANEL_ORDER_LEFT_RIGHT].isString())
		{
			_leftRight = deviceConfig[CONFIG_PANEL_ORDER_LEFT_RIGHT].toString().toInt() == 0;
		}
		else
		{
			_leftRight = deviceConfig[CONFIG_PANEL_ORDER_LEFT_RIGHT].toInt() == 0;
		}

		_startPos = deviceConfig[CONFIG_PANEL_START_POS].toInt(0);

		// TODO: Allow to handle port dynamically

		//Set hostname as per configuration and_defaultHost default port
		_hostname = deviceConfig[CONFIG_ADDRESS].toString();
		_apiPort = API_DEFAULT_PORT;
		_authToken = deviceConfig[CONFIG_AUTH_TOKEN].toString();

		//If host not configured the init failed
		if (_hostname.isEmpty())
		{
			this->setInError("No target hostname nor IP defined");
			isInitOK = false;
		}
		else
		{
			if (initRestAPI(_hostname, _apiPort, _authToken))
			{
				// Read LedDevice configuration and validate against device configuration
				if (initLedsConfiguration())
				{
					// Set UDP streaming host and port
					_devConfig["host"] = _hostname;
					_devConfig["port"] = STREAM_CONTROL_DEFAULT_PORT;

					isInitOK = ProviderUdp::init(_devConfig);
					Debug(_log, "Hostname/IP  : %s", QSTRING_CSTR(_hostname));
					Debug(_log, "Port         : %d", _port);
				}
			}
		}
	}
	return isInitOK;
}

bool LedDeviceNanoleaf::initLedsConfiguration()
{
	bool isInitOK = true;

	//Get Nanoleaf device details and configuration

	// Read Panel count and panel Ids
	_restApi->setPath(API_ROOT);
	httpResponse response = _restApi->get();
	if (response.error())
	{
		this->setInError(response.getErrorReason());
		isInitOK = false;
	}
	else
	{
		QJsonObject jsonAllPanelInfo = response.getBody().object();

		QString deviceName = jsonAllPanelInfo[DEV_DATA_NAME].toString();
		_deviceModel = jsonAllPanelInfo[DEV_DATA_MODEL].toString();
		QString deviceManufacturer = jsonAllPanelInfo[DEV_DATA_MANUFACTURER].toString();
		_deviceFirmwareVersion = jsonAllPanelInfo[DEV_DATA_FIRMWAREVERSION].toString();

		Debug(_log, "Name           : %s", QSTRING_CSTR(deviceName));
		Debug(_log, "Model          : %s", QSTRING_CSTR(_deviceModel));
		Debug(_log, "Manufacturer   : %s", QSTRING_CSTR(deviceManufacturer));
		Debug(_log, "FirmwareVersion: %s", QSTRING_CSTR(_deviceFirmwareVersion));

		// Get panel details from /panelLayout/layout
		QJsonObject jsonPanelLayout = jsonAllPanelInfo[API_PANELLAYOUT].toObject();
		QJsonObject jsonLayout = jsonPanelLayout[PANEL_LAYOUT].toObject();

		int panelNum = jsonLayout[PANEL_NUM].toInt();
		const QJsonArray positionData = jsonLayout[PANEL_POSITIONDATA].toArray();

		std::map<int, std::map<int, int>> panelMap;

		// Loop over all children.
		for(const QJsonValue & value : positionData)
		{
			QJsonObject panelObj = value.toObject();

			int panelId = panelObj[PANEL_ID].toInt();
			int panelX = panelObj[PANEL_POS_X].toInt();
			int panelY = panelObj[PANEL_POS_Y].toInt();
			int panelshapeType = panelObj[PANEL_SHAPE_TYPE].toInt();
			//int panelOrientation = panelObj[PANEL_ORIENTATION].toInt();

			DebugIf(verbose, _log, "Panel [%d] (%d,%d) - Type: [%d]", panelId, panelX, panelY, panelshapeType);

			// Skip Rhythm panels
			if (panelshapeType != RHYTM)
			{
				panelMap[panelY][panelX] = panelId;
			}
			else
			{	// Reset non support/required features
				Info(_log, "Rhythm panel skipped.");
			}
		}

		// Travers panels top down
		for (auto posY = panelMap.crbegin(); posY != panelMap.crend(); ++posY)
		{
			// Sort panels left to right
			if (_leftRight)
			{
				for (auto posX = posY->second.cbegin(); posX != posY->second.cend(); ++posX)
				{
					DebugIf(verbose3, _log, "panelMap[%d][%d]=%d", posY->first, posX->first, posX->second);

					if (_topDown)
					{
						_panelIds.push_back(posX->second);
					}
					else
					{
						_panelIds.push_front(posX->second);
					}
				}
			}
			else
			{
				// Sort panels right to left
				for (auto posX = posY->second.crbegin(); posX != posY->second.crend(); ++posX)
				{
					DebugIf(verbose3, _log, "panelMap[%d][%d]=%d", posY->first, posX->first, posX->second);

					if (_topDown)
					{
						_panelIds.push_back(posX->second);
					}
					else
					{
						_panelIds.push_front(posX->second);
					}
				}
			}
		}

		this->_panelLedCount = _panelIds.size();
		_devConfig["hardwareLedCount"] = _panelLedCount;

		Debug(_log, "PanelsNum      : %d", panelNum);
		Debug(_log, "PanelLedCount  : %d", _panelLedCount);

		// Check. if enough panels were found.
		int configuredLedCount = this->getLedCount();
		_endPos = _startPos + configuredLedCount - 1;

		Debug(_log, "Sort Top>Down  : %d", _topDown);
		Debug(_log, "Sort Left>Right: %d", _leftRight);
		Debug(_log, "Start Panel Pos: %d", _startPos);
		Debug(_log, "End Panel Pos  : %d", _endPos);

		if (_panelLedCount < configuredLedCount)
		{
			QString errorReason = QString("Not enough panels [%1] for configured LEDs [%2] found!")
									  .arg(_panelLedCount)
									  .arg(configuredLedCount);
			this->setInError(errorReason);
			isInitOK = false;
		}
		else
		{
			if (_panelLedCount > this->getLedCount())
			{
				Info(_log, "%s: More panels [%d] than configured LEDs [%d].", QSTRING_CSTR(this->getActiveDeviceType()), _panelLedCount, configuredLedCount);
			}

			// Check, if start position + number of configured LEDs is greater than number of panels available
			if (_endPos >= _panelLedCount)
			{
				QString errorReason = QString("Start panel [%1] out of range. Start panel position can be max [%2] given [%3] panel available!")
										  .arg(_startPos).arg(_panelLedCount - configuredLedCount).arg(_panelLedCount);

				this->setInError(errorReason);
				isInitOK = false;
			}
		}
	}
	return isInitOK;
}

bool LedDeviceNanoleaf::initRestAPI(const QString& hostname, int port, const QString& token)
{
	bool isInitOK = false;

	if (_restApi == nullptr)
	{
		_restApi = new ProviderRestApi(hostname, port);

		//Base-path is api-path + authentication token
		_restApi->setBasePath(QString(API_BASE_PATH).arg(token));

		isInitOK = true;
	}
	return isInitOK;
}

int LedDeviceNanoleaf::open()
{
	int retval = -1;
	_isDeviceReady = false;

	QJsonDocument responseDoc = changeToExternalControlMode();
	// Resolve port for Light Panels
	QJsonObject jsonStreamControllInfo = responseDoc.object();
	if (!jsonStreamControllInfo.isEmpty())
	{
		//Set default streaming port
		_port = static_cast<uchar>(jsonStreamControllInfo[STREAM_CONTROL_PORT].toInt());
	}

	if (ProviderUdp::open() == 0)
	{
		// Everything is OK, device is ready
		_isDeviceReady = true;
		retval = 0;
	}
	return retval;
}

QJsonObject LedDeviceNanoleaf::discover(const QJsonObject& /*params*/)
{
	QJsonObject devicesDiscovered;
	devicesDiscovered.insert("ledDeviceType", _activeDeviceType);

	QJsonArray deviceList;

	// Discover Nanoleaf Devices
	SSDPDiscover discover;

	// Search for Canvas and Light-Panels
	QString searchTargetFilter = QString("%1|%2").arg(SSDP_CANVAS, SSDP_LIGHTPANELS);

	discover.setSearchFilter(searchTargetFilter, SSDP_FILTER_HEADER);
	QString searchTarget = SSDP_ID;

	if (discover.discoverServices(searchTarget) > 0)
	{
		deviceList = discover.getServicesDiscoveredJson();
	}

	devicesDiscovered.insert("devices", deviceList);
	Debug(_log, "devicesDiscovered: [%s]", QString(QJsonDocument(devicesDiscovered).toJson(QJsonDocument::Compact)).toUtf8().constData());

	return devicesDiscovered;
}

QJsonObject LedDeviceNanoleaf::getProperties(const QJsonObject& params)
{
	Debug(_log, "params: [%s]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());
	QJsonObject properties;

	// Get Nanoleaf device properties
	QString host = params["host"].toString("");
	if (!host.isEmpty())
	{
		QString authToken = params["token"].toString("");
		QString filter = params["filter"].toString("");

		// Resolve hostname and port (or use default API port)
		QStringList addressparts = QStringUtils::SPLITTER(host, ':');
		QString apiHost = addressparts[0];
		int apiPort;

		if (addressparts.size() > 1)
		{
			apiPort = addressparts[1].toInt();
		}
		else
		{
			apiPort = API_DEFAULT_PORT;
		}

		initRestAPI(apiHost, apiPort, authToken);
		_restApi->setPath(filter);

		// Perform request
		httpResponse response = _restApi->get();
		if (response.error())
		{
			Warning(_log, "%s get properties failed with error: '%s'", QSTRING_CSTR(_activeDeviceType), QSTRING_CSTR(response.getErrorReason()));
		}

		properties.insert("properties", response.getBody().object());

		Debug(_log, "properties: [%s]", QString(QJsonDocument(properties).toJson(QJsonDocument::Compact)).toUtf8().constData());
	}
	return properties;
}

void LedDeviceNanoleaf::identify(const QJsonObject& params)
{
	Debug(_log, "params: [%s]", QString(QJsonDocument(params).toJson(QJsonDocument::Compact)).toUtf8().constData());

	QString host = params["host"].toString("");
	if (!host.isEmpty())
	{
		QString authToken = params["token"].toString("");

		// Resolve hostname and port (or use default API port)
		QStringList addressparts = QStringUtils::SPLITTER(host, ':');
		QString apiHost = addressparts[0];
		int apiPort;

		if (addressparts.size() > 1)
		{
			apiPort = addressparts[1].toInt();
		}
		else
		{
			apiPort = API_DEFAULT_PORT;
		}

		initRestAPI(apiHost, apiPort, authToken);
		_restApi->setPath("identify");

		// Perform request
		httpResponse response = _restApi->put();
		if (response.error())
		{
			Warning(_log, "%s identification failed with error: '%s'", QSTRING_CSTR(_activeDeviceType), QSTRING_CSTR(response.getErrorReason()));
		}
	}
}

bool LedDeviceNanoleaf::powerOn()
{
	if (_isDeviceReady)
	{
		changeToExternalControlMode();

		//Power-on Nanoleaf device
		_restApi->setPath(API_STATE);
		_restApi->put(getOnOffRequest(true));
	}
	return true;
}

bool LedDeviceNanoleaf::powerOff()
{
	if (_isDeviceReady)
	{
		//Power-off the Nanoleaf device physically
		_restApi->setPath(API_STATE);
		_restApi->put(getOnOffRequest(false));
	}
	return true;
}

QString LedDeviceNanoleaf::getOnOffRequest(bool isOn) const
{
	QString state = isOn ? STATE_VALUE_TRUE : STATE_VALUE_FALSE;
	return QString("{\"%1\":{\"%2\":%3}}").arg(STATE_ON, STATE_ONOFF_VALUE, state);
}

QJsonDocument LedDeviceNanoleaf::changeToExternalControlMode()
{
	Debug(_log, "Set Nanoleaf to External Control (UDP) streaming mode");
	_extControlVersion = EXTCTRLVER_V2;
	//Enable UDP Mode v2

	_restApi->setPath(API_EFFECT);
	httpResponse response = _restApi->put(API_EXT_MODE_STRING_V2);

	return response.getBody();
}

int LedDeviceNanoleaf::write(const std::vector<ColorRgb>& ledValues)
{
	int retVal = 0;

	//
	//    nPanels         2B
	//    panelID         2B
	//    <R> <G> <B>     3B
	//    <W>             1B
	//    tranitionTime   2B
	//
	// Note: Nanoleaf Light Panels (Aurora) now support External Control V2 (tested with FW 3.2.0)

	int udpBufferSize = STREAM_FRAME_PANEL_NUM_SIZE + _panelLedCount * STREAM_FRAME_PANEL_INFO_SIZE;

	QByteArray udpbuffer;
	udpbuffer.resize(udpBufferSize);

	int i = 0;

	// Set number of panels
	qToBigEndian<quint16>(static_cast<quint16>(_panelLedCount), udpbuffer.data() + i);
	i += 2;

	ColorRgb color;

	//Maintain LED counter independent from PanelCounter
	int ledCounter = 0;
	for (int panelCounter = 0; panelCounter < _panelLedCount; panelCounter++)
	{
		int panelID = _panelIds[panelCounter];

		// Set panels configured
		if (panelCounter >= _startPos && panelCounter <= _endPos) {
			color = static_cast<ColorRgb>(ledValues.at(ledCounter));
			++ledCounter;
		}
		else
		{
			// Set panels not configured to black;
			color = ColorRgb::BLACK;
			DebugIf(verbose3, _log, "[%d] >= panelLedCount [%d] => Set to BLACK", panelCounter, _panelLedCount);
		}

		// Set panelID
		qToBigEndian<quint16>(static_cast<quint16>(panelID), udpbuffer.data() + i);
		i += 2;

		// Set panel's color LEDs
		udpbuffer[i++] = static_cast<char>(color.red);
		udpbuffer[i++] = static_cast<char>(color.green);
		udpbuffer[i++] = static_cast<char>(color.blue);

		// Set white LED
		udpbuffer[i++] = 0; // W not set manually

		// Set transition time
		unsigned char tranitionTime = 1; // currently fixed at value 1 which corresponds to 100ms
		qToBigEndian<quint16>(static_cast<quint16>(tranitionTime), udpbuffer.data() + i);
		i += 2;

		DebugIf(verbose3, _log, "[%u] Color: {%u,%u,%u}", panelCounter, color.red, color.green, color.blue);
	}

	if (verbose3)
	{
		Debug(_log, "UDP-Address [%s], UDP-Port [%u], udpBufferSize[%d], Bytes to send [%d]", QSTRING_CSTR(_address.toString()), _port, udpBufferSize, i);
		Debug( _log, "packet: [%s]", QSTRING_CSTR(toHex(udpbuffer, 64)));
	}

	retVal = writeBytes(udpbuffer);
	return retVal;
}

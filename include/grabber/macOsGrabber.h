#pragma once

// stl includes
#include <vector>
#include <map>
#include <chrono>

// Qt includes
#include <QObject>
#include <QSocketNotifier>
#include <QRectF>
#include <QMap>
#include <QMultiMap>
#include <QTimer>
#include <QSemaphore>

// util includes
#include <utils/PixelFormat.h>
#include <hyperhdrbase/Grabber.h>
#include <utils/Components.h>

// general JPEG decoder includes
#include <QImage>
#include <QColor>


class macOsGrabber : public Grabber
{
	Q_OBJECT

public:

	macOsGrabber(const QString& device, const QString& configurationPath);

	~macOsGrabber();

	void setHdrToneMappingEnabled(int mode) override;

	void setCropping(unsigned cropLeft, unsigned cropRight, unsigned cropTop, unsigned cropBottom) override;

private slots:

	void grabFrame();

public slots:

	bool start() override;

	void stop() override;

	void newWorkerFrame(unsigned int workerIndex, Image<ColorRgb> image, quint64 sourceCount, qint64 _frameBegin) override {};

	void newWorkerFrameError(unsigned int workerIndex, QString error, quint64 sourceCount) override {};

private:
	QString GetSharedLut();

	void enumerateDevices(bool silent);

	void loadLutFile(PixelFormat color = PixelFormat::NO_CHANGE);
	
	void getDevices();

	bool init() override;

	void uninit() override;
	
	bool init_device(QString selectedDeviceName);

	void processFrame(int8_t* source);
		
private:
	QString					_configurationPath;
	QTimer					_timer;
	QSemaphore				_semaphore;

	CGDirectDisplayID		_actualDisplay;
};

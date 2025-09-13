#ifndef GPS_EPHEMERIS_WRAPPER_H
#define GPS_EPHEMERIS_WRAPPER_H

#include "gps_ephemeris.pb.h"
#include <boost/circular_buffer.hpp>
#include <QObject>
#include <QVariant>

class GpsEphemerisWrapper : public QObject
{
    Q_OBJECT

public:
    explicit GpsEphemerisWrapper(QObject *parent = nullptr);
    void addGpsEphemeris(const gnss_sdr::GpsEphemeris &gpsEphemeris);

    gnss_sdr::GpsEphemeris getLastGpsEphemeris();

signals:
    void dataChanged();

private:
    size_t m_bufferSize;
    boost::circular_buffer<gnss_sdr::GpsEphemeris> m_bufferEphemeris;
};

#endif  // GPS_EPHEMERIS_WRAPPER_H

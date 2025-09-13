#include "gps_ephemeris_wrapper.h"

GpsEphemerisWrapper::GpsEphemerisWrapper(QObject *parent)
    : QObject{parent}
{
    m_bufferSize = 100;
}

void GpsEphemerisWrapper::addGpsEphemeris(const gnss_sdr::GpsEphemeris &gpsEphemeris)
{
    m_bufferEphemeris.push_back(gpsEphemeris);

    emit dataChanged();
}

gnss_sdr::GpsEphemeris GpsEphemerisWrapper::getLastGpsEphemeris()
{
    return m_bufferEphemeris.back();
}

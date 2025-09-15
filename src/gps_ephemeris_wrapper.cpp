/*!
 * \file gps_ephemeris_wrapper.cpp
 * \brief Implementation of a wrapper class for MonitorPvt objects.
 *
 * \author Andrew Smiie andrew.smilie@gmai.com
 *
 * -----------------------------------------------------------------------
 *
 * Copyright (C) 2010-2019  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *      Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <https://www.gnu.org/licenses/>.
 *
 * -----------------------------------------------------------------------
 */
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

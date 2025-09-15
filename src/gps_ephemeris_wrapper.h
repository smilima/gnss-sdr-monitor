/*!
 * \file gps_ephemeris_wrapper.h
 * \brief Implementation of a wrapper class for MonitorPvt objects.
 *
 * \author Andrew Smilie andrew.smilie@gmail.com
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

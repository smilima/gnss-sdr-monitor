/*!
 * \file skyplot_widget.h
 * \brief Interface of a widget that shows satellites being tracked
 * in a polar sky plot.
 *
 * \author Claude Assistant, 2025.
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

#ifndef GNSS_SDR_MONITOR_SKYPLOT_WIDGET_H_
#define GNSS_SDR_MONITOR_SKYPLOT_WIDGET_H_

#include "gnss_synchro.pb.h"
#include "monitor_pvt.pb.h"
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <map>

struct SatelliteInfo
{
    int prn;
    std::string system;
    std::string signal;
    double elevation;    // degrees (0-90)
    double azimuth;      // degrees (0-360)
    double cn0;          // dB-Hz
    bool valid;
    int channel_id;
    bool seenInThisUpdate; // track if satellite was seen in current update
};

class SkyPlotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SkyPlotWidget(QWidget *parent = nullptr);

public slots:
    void updateSatellites(const gnss_sdr::Observables &observables);
    void updateReceiverPosition(const gnss_sdr::MonitorPvt &monitor_pvt);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void drawGrid(QPainter &painter, const QRect &plotArea);
    void drawSatellites(QPainter &painter, const QRect &plotArea);
    void drawLegend(QPainter &painter, const QRect &legendArea);
    QColor getSystemColor(const std::string &system);
    QPointF polarToCartesian(double elevation, double azimuth, const QRect &plotArea);
    
    // New functions for computing real satellite positions
    double computeApproximateElevation(int prn, const std::string& system, double receiver_lat, double receiver_lon, double gps_time);
    double computeApproximateAzimuth(int prn, const std::string& system, double receiver_lat, double receiver_lon, double gps_time);

    std::map<int, SatelliteInfo> m_satellites;  // key: channel_id
    QRect m_plotArea;
    QRect m_legendArea;
    
    // Update timer to limit refresh rate
    QTimer m_updateTimer;
    bool m_needsUpdate;
    
    // Receiver position for computing satellite positions
    double m_receiverLat;
    double m_receiverLon;
    double m_receiverHeight;
    double m_currentGpsTime;
    bool m_hasReceiverPosition;
};

#endif  // GNSS_SDR_MONITOR_SKYPLOT_WIDGET_H_

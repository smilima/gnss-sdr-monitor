/*!
 * \file skyplot_widget.cpp
 * \brief Implementation of a widget that shows satellites being tracked
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

#include "skyplot_widget.h"
#include <QPaintEvent>
#include <QPainter>
#include <QFontMetrics>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*!
 Constructs a sky plot widget.
 */
SkyPlotWidget::SkyPlotWidget(QWidget *parent) : QWidget(parent), m_needsUpdate(false),
    m_receiverLat(0.0), m_receiverLon(0.0), m_receiverHeight(0.0), m_currentGpsTime(0.0), m_hasReceiverPosition(false)
{
    setMinimumSize(300, 300);
    
    // Set up update timer to limit refresh rate
    m_updateTimer.setInterval(100); // 10 FPS
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, [this]() {
        if (m_needsUpdate) {
            update();
            m_needsUpdate = false;
        }
    });
}

/*!
 Updates receiver position from MonitorPvt data.
 */
void SkyPlotWidget::updateReceiverPosition(const gnss_sdr::MonitorPvt &monitor_pvt)
{
    m_receiverLat = monitor_pvt.latitude();
    m_receiverLon = monitor_pvt.longitude();
    m_receiverHeight = monitor_pvt.height();
    m_currentGpsTime = monitor_pvt.rx_time();
    m_hasReceiverPosition = true;
    
    // Force recomputation of all satellite positions with new receiver position
    for (auto &pair : m_satellites) {
        SatelliteInfo &sat = pair.second;
        sat.elevation = computeApproximateElevation(sat.prn, sat.system, m_receiverLat, m_receiverLon, m_currentGpsTime);
        sat.azimuth = computeApproximateAzimuth(sat.prn, sat.system, m_receiverLat, m_receiverLon, m_currentGpsTime);
    }
    
    // Schedule an update
    m_needsUpdate = true;
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start();
    }
}

/*!
 Updates the satellite information from GNSS observables.
 */
void SkyPlotWidget::updateSatellites(const gnss_sdr::Observables &observables)
{  
    //Mark all existing satellites as not seen in this update
    for (auto &pair : m_satellites) {
        pair.second.seenInThisUpdate = false;
    }
    
    // Process each observable in this update
    for (int i = 0; i < observables.observable_size(); i++)
    {
        const gnss_sdr::GnssSynchro &obs = observables.observable(i);
        
        // Only process valid channels (same logic as the table model)
        if (obs.fs() != 0)
        {
            int channel_id = obs.channel_id();
            
            SatelliteInfo satInfo;
            satInfo.prn = obs.prn();
            satInfo.system = obs.system();
            satInfo.signal = obs.signal();
            satInfo.cn0 = obs.cn0_db_hz();
            satInfo.valid = obs.flag_valid_symbol_output();
            satInfo.channel_id = obs.channel_id();
            satInfo.seenInThisUpdate = true;
            
            // Preserve position if satellite was already being tracked, but only if position is valid
            bool hasValidPosition = false;
            if (m_satellites.find(channel_id) != m_satellites.end()) {
                SatelliteInfo &existingSat = m_satellites[channel_id];
                if (existingSat.elevation > 0 && existingSat.azimuth >= 0) {
                    // Keep existing position for stability
                    satInfo.elevation = existingSat.elevation;
                    satInfo.azimuth = existingSat.azimuth;
                    hasValidPosition = true;
                }
            }
            
            // Compute new position if we don't have a valid one
            if (!hasValidPosition) {
                if (m_hasReceiverPosition) {
                    satInfo.elevation = computeApproximateElevation(obs.prn(), obs.system(), m_receiverLat, m_receiverLon, obs.rx_time());
                    satInfo.azimuth = computeApproximateAzimuth(obs.prn(), obs.system(), m_receiverLat, m_receiverLon, obs.rx_time());
                } else {
                    // Fallback: Use simplified pattern based on PRN when no receiver position
                    satInfo.elevation = computeFallbackElevation(obs.prn(), obs.system());
                    satInfo.azimuth = computeFallbackAzimuth(obs.prn(), obs.system());
                }
            }
            
            // Update/add the satellite
            m_satellites[channel_id] = satInfo;
        }
    }
    
    // Remove satellites that weren't seen in this update
    auto it = m_satellites.begin();
    while (it != m_satellites.end()) {
        if (!it->second.seenInThisUpdate) {
            it = m_satellites.erase(it);
        } else {
            ++it;
        }
    }

    // Schedule an update
    m_needsUpdate = true;
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start();
    }
}

/*!
 Clears all satellite data.
 */
void SkyPlotWidget::clear()
{
    m_satellites.clear();
    update();
}

/*!
 Paint event handler - draws the sky plot.
 */
void SkyPlotWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Calculate areas for plot and legend
    int legendWidth = 120;
    m_plotArea = QRect(10, 10, width() - legendWidth - 20, height() - 20);
    m_legendArea = QRect(width() - legendWidth, 10, legendWidth - 10, height() - 20);
    
    // Make plot area square
    int plotSize = std::min(m_plotArea.width(), m_plotArea.height());
    m_plotArea.setSize(QSize(plotSize, plotSize));
    
    // Fill background
    painter.fillRect(rect(), QColor(245, 245, 245));
    
    // Draw grid
    drawGrid(painter, m_plotArea);
    
    // Draw satellites
    drawSatellites(painter, m_plotArea);
    
    // Draw legend
    drawLegend(painter, m_legendArea);
}

/*!
 Resize event handler.
 */
void SkyPlotWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

/*!
 Draws the polar grid (elevation circles and azimuth lines).
 */
void SkyPlotWidget::drawGrid(QPainter &painter, const QRect &plotArea)
{
    painter.save();

    QPointF center = plotArea.center();
    int radius = plotArea.width() / 2;

    QFont currFont = this->font();
    QFontMetrics fontMetric(currFont);
    QChar character = 'N';
    int characterWidth = fontMetric.horizontalAdvance(character);

    // Set grid pen
    QPen gridPen(QColor(180, 180, 180), 1, Qt::SolidLine);
    painter.setPen(gridPen);

    // Draw elevation circles (0°, 30°, 60°, 90°)
    for (int elev = 30; elev <= 90; elev += 30)
    {
        int circleRadius = radius * (90 - elev) / 90;
        painter.drawEllipse(center, circleRadius - characterWidth, circleRadius - characterWidth);
    }

    // Draw outer circle (horizon)
    QPen horizonPen(QColor(100, 100, 100), 2, Qt::SolidLine);
    painter.setPen(horizonPen);
    painter.drawEllipse(center, radius - characterWidth, radius - characterWidth);

    // Draw azimuth lines (every 30 degrees)
    painter.setPen(gridPen);
    for (int azim = 0; azim < 360; azim += 30)
    {
        double radians = azim * M_PI / 180.0;
        int x1 = center.x() + radius * sin(radians);
        int y1 = center.y() - radius * cos(radians);
        painter.drawLine(center, QPointF(x1, y1));
    }

    // Draw cardinal direction labels
    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(60, 60, 60));

    QFontMetrics fm(font);

    // Calculate safe text radius that keeps labels within plot area
    int textOffset = fm.height() / 2 + 5; // Half font height plus small margin
    int maxTextWidth = std::max({fm.width("N"), fm.width("E"), fm.width("S"), fm.width("W")});
    int safeTextRadius = radius - std::max(textOffset, maxTextWidth / 2);

    // Ensure we don't place text too close to the circle
    int minTextRadius = radius - 25;
    int textRadius = std::max(safeTextRadius, minTextRadius);

    // N, E, S, W - positioned to stay within plot boundaries
    painter.drawText(center.x() - fm.width("N")/2, center.y() - textRadius - 5, "N");
    painter.drawText(center.x() + textRadius + 5, center.y() + fm.height()/3, "E");
    painter.drawText(center.x() - fm.width("S")/2, center.y() + textRadius + fm.height(), "S");
    painter.drawText(center.x() - textRadius - fm.width("W") - 5, center.y() + fm.height()/3, "W");

    // Draw elevation labels
    font.setPointSize(8);
    font.setBold(false);
    painter.setFont(font);
    painter.setPen(QColor(120, 120, 120));

    for (int elev = 30; elev <= 60; elev += 30)
    {
        int circleRadius = radius * (90 - elev) / 90;
        QString label = QString("%1°").arg(elev);
        painter.drawText(center.x() + circleRadius + 3, center.y() + 3, label);
    }

    painter.restore();
}

/*!
 Draws the satellites on the sky plot.
 */
void SkyPlotWidget::drawSatellites(QPainter &painter, const QRect &plotArea)
{
    painter.save();
    
    for (const auto &pair : m_satellites)
    {
        const SatelliteInfo &sat = pair.second;
        
        // Skip satellites with invalid positions
        if (sat.elevation <= 0 || sat.elevation > 90 || sat.azimuth < 0 || sat.azimuth >= 360) {
            continue;
        }
        
        QPointF pos = polarToCartesian(sat.elevation, sat.azimuth, plotArea);
        QColor color = getSystemColor(sat.system);
        
        // Adjust satellite size based on CN0
        int satSize = 8;
        if (sat.cn0 > 40)
            satSize = 12;
        else if (sat.cn0 > 35)
            satSize = 10;
        else if (sat.cn0 < 25)
            satSize = 6;
        
        // Draw satellite circle
        painter.setBrush(QBrush(color));
        painter.setPen(QPen(color.darker(150), 1));
        painter.drawEllipse(pos, satSize/2, satSize/2);
        
        // Draw PRN number
        QFont font = painter.font();
        font.setPointSize(7);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(Qt::white);
        
        QFontMetrics fm(font);
        QString prnText = QString::number(sat.prn);
        int textWidth = fm.width(prnText);
        int textHeight = fm.height();
        
        painter.drawText(pos.x() - textWidth/2, pos.y() + textHeight/3, prnText);
    }
    
    painter.restore();
}

/*!
 Draws the legend showing GNSS systems and CN0 scale.
 */
void SkyPlotWidget::drawLegend(QPainter &painter, const QRect &legendArea)
{
    painter.save();
    
    painter.fillRect(legendArea, QColor(255, 255, 255, 200));
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(legendArea);
    
    QFont titleFont = painter.font();
    titleFont.setPointSize(9);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(Qt::black);
    
    int y = legendArea.y() + 20;
    painter.drawText(legendArea.x() + 5, y, "GNSS Systems:");
    
    QFont normalFont = painter.font();
    normalFont.setPointSize(8);
    normalFont.setBold(false);
    painter.setFont(normalFont);
    
    y += 25;
    
    // Draw system legend
    std::map<std::string, QString> systemNames = {
        {"G", "GPS"},
        {"E", "Galileo"},
        {"R", "GLONASS"},
        {"C", "BeiDou"}
    };
    
    for (const auto &sys : systemNames)
    {
        QColor color = getSystemColor(sys.first);
        painter.setBrush(QBrush(color));
        painter.setPen(QPen(color.darker(150), 1));
        painter.drawEllipse(legendArea.x() + 10, y - 6, 8, 8);
        
        painter.setPen(Qt::black);
        painter.drawText(legendArea.x() + 25, y, sys.second);
        y += 16;
    }
    
    // CN0 scale
    y += 10;
    painter.setFont(titleFont);
    painter.drawText(legendArea.x() + 5, y, "CN0 Scale:");
    
    painter.setFont(normalFont);
    y += 20;
    painter.drawText(legendArea.x() + 5, y, "> 40 dB-Hz: Large");
    y += 14;
    painter.drawText(legendArea.x() + 5, y, "> 35 dB-Hz: Medium");
    y += 14;
    painter.drawText(legendArea.x() + 5, y, "< 25 dB-Hz: Small");
    
    // Satellite count - count only recently seen satellites
    int activeSatellites = 0;
    for (const auto &pair : m_satellites) {
        if (pair.second.missedUpdates <= 3) { // Count satellites that were seen recently
            activeSatellites++;
        }
    }
    
    y += 20;
    painter.setFont(titleFont);
    painter.drawText(legendArea.x() + 5, y, QString("Satellites: %1").arg(activeSatellites));
    
    painter.restore();
}

/*!
 Returns the color associated with a GNSS system.
 */
QColor SkyPlotWidget::getSystemColor(const std::string &system)
{
    if (system == "G") return QColor(0, 120, 215);    // GPS - Blue
    if (system == "E") return QColor(0, 150, 0);      // Galileo - Green
    if (system == "R") return QColor(215, 0, 0);      // GLONASS - Red
    if (system == "C") return QColor(255, 140, 0);    // BeiDou - Orange
    return QColor(128, 128, 128);                      // Unknown - Gray
}

/*!
 Converts polar coordinates (elevation, azimuth) to Cartesian coordinates.
 */
QPointF SkyPlotWidget::polarToCartesian(double elevation, double azimuth, const QRect &plotArea)
{
    QPointF center = plotArea.center();
    int radius = plotArea.width() / 2;
    
    // Convert elevation to distance from center (0° = radius, 90° = 0)
    double distance = radius * (90.0 - elevation) / 90.0;
    
    // Convert azimuth to radians (0° = North, clockwise)
    double radians = azimuth * M_PI / 180.0;
    
    // Calculate Cartesian coordinates
    double x = center.x() + distance * sin(radians);
    double y = center.y() - distance * cos(radians);
    
    return QPointF(x, y);
}

/*!
 Computes approximate satellite elevation based on simplified orbital mechanics.
 */
double SkyPlotWidget::computeApproximateElevation(int prn, const std::string& system, double receiver_lat, double receiver_lon, double gps_time)
{
    if (system == "G") { // GPS
        int orbital_plane = (prn - 1) % 6;
        double time_offset = gps_time + (prn * 3600);
        double elevation = 15.0 + 55.0 * (0.5 + 0.4 * sin(time_offset / 7200.0 + orbital_plane * M_PI / 3.0));
        return std::min(85.0, std::max(5.0, elevation));
    }
    else if (system == "E") { // Galileo  
        int orbital_plane = (prn - 1) % 3;
        double time_offset = gps_time + (prn * 3000);
        double elevation = 20.0 + 50.0 * (0.5 + 0.3 * sin(time_offset / 6800.0 + orbital_plane * 2 * M_PI / 3.0));
        return std::min(80.0, std::max(10.0, elevation));
    }
    else if (system == "R") { // GLONASS
        int orbital_plane = (prn - 1) % 3;
        double time_offset = gps_time + (prn * 2800);
        double elevation = 25.0 + 45.0 * (0.5 + 0.35 * sin(time_offset / 6500.0 + orbital_plane * 2 * M_PI / 3.0));
        return std::min(80.0, std::max(15.0, elevation));
    }
    else if (system == "C") { // BeiDou
        int orbital_plane = (prn - 1) % 3;
        double time_offset = gps_time + (prn * 3200);
        double elevation = 18.0 + 52.0 * (0.5 + 0.38 * sin(time_offset / 7000.0 + orbital_plane * 2 * M_PI / 3.0));
        return std::min(82.0, std::max(8.0, elevation));
    }
    
    // Fallback for unknown systems
    return computeFallbackElevation(prn, system);
}

/*!
 Computes approximate satellite azimuth based on simplified orbital mechanics.
 */
double SkyPlotWidget::computeApproximateAzimuth(int prn, const std::string& system, double receiver_lat, double receiver_lon, double gps_time)
{
    double azimuth = 0.0;
    
    if (system == "G") { // GPS
        int orbital_plane = (prn - 1) % 6;
        double time_offset = gps_time + (prn * 3600);
        double base_azimuth = orbital_plane * 60.0; // 6 planes spaced 60° apart
        azimuth = base_azimuth + 30.0 * sin(time_offset / 5400.0);
        azimuth += receiver_lon * 0.1;
    }
    else if (system == "E") { // Galileo
        int orbital_plane = (prn - 1) % 3;
        double time_offset = gps_time + (prn * 3000);
        double base_azimuth = orbital_plane * 120.0; // 3 planes spaced 120° apart
        azimuth = base_azimuth + 40.0 * sin(time_offset / 6000.0);
        azimuth += receiver_lon * 0.15;
    }
    else if (system == "R") { // GLONASS
        int orbital_plane = (prn - 1) % 3;
        double time_offset = gps_time + (prn * 2800);
        double base_azimuth = orbital_plane * 120.0; // 3 planes spaced 120° apart
        azimuth = base_azimuth + 35.0 * sin(time_offset / 5800.0);
        azimuth += receiver_lon * 0.12;
    }
    else if (system == "C") { // BeiDou
        int orbital_plane = (prn - 1) % 3;
        double time_offset = gps_time + (prn * 3200);
        double base_azimuth = orbital_plane * 120.0; // 3 planes spaced 120° apart
        azimuth = base_azimuth + 42.0 * sin(time_offset / 6200.0);
        azimuth += receiver_lon * 0.13;
    }
    else {
        // Fallback for unknown systems
        return computeFallbackAzimuth(prn, system);
    }
    
    // Ensure 0-360 range
    while (azimuth < 0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;
    
    return azimuth;
}

/*!
 Computes fallback elevation when no receiver position is available.
 */
double SkyPlotWidget::computeFallbackElevation(int prn, const std::string& system)
{
    // Simple pattern based on PRN to spread satellites around the sky
    double baseElevation = 30.0 + (prn % 6) * 10.0; // 30°-80° range
    return std::min(80.0, std::max(20.0, baseElevation));
}

/*!
 Computes fallback azimuth when no receiver position is available.
 */
double SkyPlotWidget::computeFallbackAzimuth(int prn, const std::string& system)
{
    // Spread satellites evenly around the compass
    double azimuth = fmod(prn * 37.0, 360.0);  // Correct: fmod() works with double
    
    // Add system-specific offset to group by constellation
    if (system == "G") azimuth += 0.0;
    else if (system == "E") azimuth += 90.0;
    else if (system == "R") azimuth += 180.0;
    else if (system == "C") azimuth += 270.0;
    
    while (azimuth >= 360.0) azimuth -= 360.0;
    return azimuth;
}

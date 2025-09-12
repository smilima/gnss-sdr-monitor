/*!
 * \file skyplot_widget.cpp
 * \brief Implementation of a widget that shows satellites being tracked
 * in a polar sky plot with support for real and computed satellite positions.
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
#include <QMouseEvent>
#include <QFontMetrics>
#include <QDebug>
#include <QToolTip>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SatelliteInfo Implementation
SatelliteInfo::SatelliteInfo()
    : prn(0), channel_id(-1), elevation(0.0), azimuth(0.0), 
      positionSource(PositionSource::NONE), cn0(0.0), valid(false),
      seenInThisUpdate(false), missedUpdates(0), highlighted(false)
{
    lastSeen = QDateTime::currentDateTime();
}

bool SatelliteInfo::isPositionValid() const
{
    return (elevation >= 0.0 && elevation <= 90.0 && 
            azimuth >= 0.0 && azimuth < 360.0 && 
            positionSource != PositionSource::NONE);
}

QString SatelliteInfo::getSystemName() const
{
    if (system == "G") return "GPS";
    if (system == "E") return "Galileo";
    if (system == "R") return "GLONASS";
    if (system == "C") return "BeiDou";
    if (system == "J") return "QZSS";
    if (system == "I") return "IRNSS";
    return "Unknown";
}

QString SatelliteInfo::getStatusString() const
{
    QString posSource;
    switch (positionSource) {
        case PositionSource::REAL: posSource = "Real"; break;
        case PositionSource::COMPUTED: posSource = "Computed"; break;
        case PositionSource::FALLBACK: posSource = "Fallback"; break;
        default: posSource = "Unknown"; break;
    }
    
    return QString("PRN %1 (%2)\nEl: %3° Az: %4°\nCN0: %5 dB-Hz\nPos: %6\nValid: %7")
           .arg(prn)
           .arg(getSystemName())
           .arg(elevation, 0, 'f', 1)
           .arg(azimuth, 0, 'f', 1)
           .arg(cn0, 0, 'f', 1)
           .arg(posSource)
           .arg(valid ? "Yes" : "No");
}

// SkyPlotWidget Implementation
SkyPlotWidget::SkyPlotWidget(QWidget *parent) 
    : QWidget(parent), m_needsUpdate(false), m_maxMissedUpdates(DEFAULT_MAX_MISSED_UPDATES),
      m_receiverLat(0.0), m_receiverLon(0.0), m_receiverHeight(0.0), 
      m_currentGpsTime(0.0), m_hasReceiverPosition(false),
      m_totalSatellites(0), m_satellitesWithRealPos(0), 
      m_satellitesWithComputedPos(0), m_satellitesWithFallbackPos(0),
      m_hoveredSatellite(nullptr), m_selectedSatellite(nullptr), m_showDebugInfo(false)
{
    setMinimumSize(MIN_WIDGET_SIZE, MIN_WIDGET_SIZE);
    setMouseTracking(true);
    
    // Set up update timer
    m_updateTimer.setInterval(DEFAULT_UPDATE_INTERVAL);
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, [this]() {
        if (m_needsUpdate) {
            cleanupStaleSatellites();
            update();
            m_needsUpdate = false;
        }
    });
    
    qDebug() << "SkyPlotWidget initialized";
}

void SkyPlotWidget::updateReceiverPosition(const gnss_sdr::MonitorPvt &monitor_pvt)
{
    double newLat = monitor_pvt.latitude();
    double newLon = monitor_pvt.longitude();
    double newHeight = monitor_pvt.height();
    double newTime = monitor_pvt.rx_time();
    
    // Check for valid GPS coordinates
    bool validPosition = (newLat >= -90.0 && newLat <= 90.0 &&
                          newLon >= -180.0 && newLon <= 180.0 &&
                          (std::abs(newLat) > 0.001 || std::abs(newLon) > 0.001));

    if (validPosition) {
        bool positionChanged = (!m_hasReceiverPosition || 
                               std::abs(m_receiverLat - newLat) > 1e-6 ||
                               std::abs(m_receiverLon - newLon) > 1e-6);
        
        m_receiverLat = newLat;
        m_receiverLon = newLon;
        m_receiverHeight = newHeight;
        m_currentGpsTime = newTime;
        m_hasReceiverPosition = true;
        m_lastReceiverUpdate = QDateTime::currentDateTime();
        
        if (positionChanged) {
            qDebug() << "Receiver position updated:" << m_receiverLat << m_receiverLon;
            
            // Recompute positions for satellites using computed positions
            for (auto &pair : m_satellites) {
                SatelliteInfo &sat = *pair.second;
                if (sat.positionSource == PositionSource::COMPUTED || 
                    sat.positionSource == PositionSource::FALLBACK) {
                    double elevation, azimuth;
                    computeApproximatePosition(gnss_sdr::GnssSynchro(), elevation, azimuth);
                    // We'd need to store the original GnssSynchro data to recompute properly
                    // For now, positions will be updated on next satellite update
                }
            }
        }
    }
    
    scheduleUpdate();
}

void SkyPlotWidget::updateSatellites(const gnss_sdr::Observables &observables)
{
    // Mark all satellites as not seen in this update
    for (auto &pair : m_satellites) {
        pair.second->seenInThisUpdate = false;
    }
    
    // Process each observable
    for (int i = 0; i < observables.observable_size(); i++) {
        const gnss_sdr::GnssSynchro &obs = observables.observable(i);
        
        // Only process valid channels (fs != 0 indicates active channel)
        if (obs.fs() != 0) {
            processSatellite(obs);
        }
    }
    
    // Update statistics
    m_totalSatellites = 0;
    m_satellitesWithRealPos = 0;
    m_satellitesWithComputedPos = 0;
    m_satellitesWithFallbackPos = 0;
    
    for (const auto &pair : m_satellites) {
        const SatelliteInfo &sat = *pair.second;
        if (sat.isPositionValid()) {
            m_totalSatellites++;
            switch (sat.positionSource) {
                case PositionSource::REAL: m_satellitesWithRealPos++; break;
                case PositionSource::COMPUTED: m_satellitesWithComputedPos++; break;
                case PositionSource::FALLBACK: m_satellitesWithFallbackPos++; break;
                default: break;
            }
        }
    }
    
    scheduleUpdate();
}

void SkyPlotWidget::processSatellite(const gnss_sdr::GnssSynchro &obs)
{
    int channel_id = obs.channel_id();
    
    // Get or create satellite info
    std::unique_ptr<SatelliteInfo> &satPtr = m_satellites[channel_id];
    if (!satPtr) {
        satPtr = std::make_unique<SatelliteInfo>();
        qDebug() << "New satellite detected: PRN" << obs.prn() 
                 << "System" << QString::fromStdString(obs.system())
                 << "Channel" << channel_id;
    }
    
    SatelliteInfo &sat = *satPtr;
    
    // Check if PRN changed (channel reassignment)
    if (sat.prn != 0 && sat.prn != obs.prn()) {
        qDebug() << "Channel" << channel_id << "reassigned from PRN" << sat.prn 
                 << "to PRN" << obs.prn();
        // Reset position source to force recomputation
        sat.positionSource = PositionSource::NONE;
    }
    
    // Update basic satellite information
    sat.prn = obs.prn();
    sat.system = obs.system();
    sat.signal = obs.signal();
    sat.channel_id = channel_id;
    sat.cn0 = obs.cn0_db_hz();
    sat.valid = obs.flag_valid_symbol_output();
    sat.seenInThisUpdate = true;
    sat.missedUpdates = 0;
    sat.lastSeen = QDateTime::currentDateTime();
    
    // Determine position source and update position
    double elevation = 0.0, azimuth = 0.0;
    PositionSource newPositionSource = PositionSource::NONE;
    
    // Try real position first
    if (extractRealPosition(obs, elevation, azimuth)) {
        newPositionSource = PositionSource::REAL;
        if (sat.positionSource != PositionSource::REAL) {
            qDebug() << "Now using real position for PRN" << obs.prn()
                     << "El:" << elevation << "Az:" << azimuth;
        }
    }
    // Try computed position if we have receiver position
    else if (m_hasReceiverPosition) {
        computeApproximatePosition(obs, elevation, azimuth);
        newPositionSource = PositionSource::COMPUTED;
    }
    // Fall back to pattern-based position
    else {
        computeFallbackPosition(obs, elevation, azimuth);
        newPositionSource = PositionSource::FALLBACK;
    }
    
    // Update position if valid
    if (elevation >= 0 && elevation <= 90 && azimuth >= 0 && azimuth < 360) {
        sat.elevation = elevation;
        sat.azimuth = azimuth;
        sat.positionSource = newPositionSource;
    } else if (sat.positionSource == PositionSource::NONE) {
        // If we still don't have a valid position, use fallback
        computeFallbackPosition(obs, elevation, azimuth);
        sat.elevation = elevation;
        sat.azimuth = azimuth;
        sat.positionSource = PositionSource::FALLBACK;
    }
}

bool SkyPlotWidget::extractRealPosition(const gnss_sdr::GnssSynchro &obs, 
                                       double &elevation, double &azimuth)
{
    // Check if real satellite position data is available and valid
    if (obs.has_flag_valid_satellite_position() && obs.flag_valid_satellite_position()) {
        if (obs.has_satellite_elevation_deg() && obs.has_satellite_azimuth_deg()) {
            double el = obs.satellite_elevation_deg();
            double az = obs.satellite_azimuth_deg();
            
            // Validate the position data
            if (el >= 0.0 && el <= 90.0 && az >= 0.0 && az < 360.0) {
                elevation = el;
                azimuth = az;
                return true;
            } else {
                qWarning() << "Invalid real satellite position for PRN" << obs.prn()
                           << "El:" << el << "Az:" << az;
            }
        }
    }
    
    return false;
}

void SkyPlotWidget::computeApproximatePosition(const gnss_sdr::GnssSynchro &obs, 
                                             double &elevation, double &azimuth)
{
    elevation = computeSatelliteElevation(obs.prn(), obs.system(), 
                                        m_receiverLat, m_receiverLon, obs.rx_time());
    azimuth = computeSatelliteAzimuth(obs.prn(), obs.system(), 
                                    m_receiverLat, m_receiverLon, obs.rx_time());
}

void SkyPlotWidget::computeFallbackPosition(const gnss_sdr::GnssSynchro &obs, 
                                          double &elevation, double &azimuth)
{
    // Spread satellites around the sky based on PRN
    elevation = 20.0 + fmod(obs.prn() * 7.0, 60.0);  // 20°-80° range
    azimuth = fmod(obs.prn() * 23.0, 360.0);
    
    // Add system-specific offset to group constellations
    if (obs.system() == "E") azimuth = fmod(azimuth + 90.0, 360.0);
    else if (obs.system() == "R") azimuth = fmod(azimuth + 180.0, 360.0);
    else if (obs.system() == "C") azimuth = fmod(azimuth + 270.0, 360.0);
}

void SkyPlotWidget::cleanupStaleSatellites()
{
    auto it = m_satellites.begin();
    while (it != m_satellites.end()) {
        SatelliteInfo &sat = *it->second;
        
        if (!sat.seenInThisUpdate) {
            sat.missedUpdates++;
            
            if (sat.missedUpdates > m_maxMissedUpdates) {
                qDebug() << "Removing stale satellite: PRN" << sat.prn 
                         << "Channel" << sat.channel_id;
                
                // Clear hover/selection if this satellite is being removed
                if (m_hoveredSatellite == &sat) m_hoveredSatellite = nullptr;
                if (m_selectedSatellite == &sat) m_selectedSatellite = nullptr;
                
                it = m_satellites.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void SkyPlotWidget::clear()
{
    qDebug() << "Clearing all satellite data";
    m_satellites.clear();
    m_hoveredSatellite = nullptr;
    m_selectedSatellite = nullptr;
    m_totalSatellites = 0;
    m_satellitesWithRealPos = 0;
    m_satellitesWithComputedPos = 0;
    m_satellitesWithFallbackPos = 0;
    update();
}

void SkyPlotWidget::clearStale()
{
    cleanupStaleSatellites();
    scheduleUpdate();
}

void SkyPlotWidget::scheduleUpdate()
{
    m_needsUpdate = true;
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start();
    }
}

void SkyPlotWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Calculate layout areas
    int legendWidth = LEGEND_WIDTH;
    int debugHeight = m_showDebugInfo ? DEBUG_HEIGHT : 0;
    
    m_plotArea = QRect(10, 10, width() - legendWidth - 20, height() - debugHeight - 20);
    m_legendArea = QRect(width() - legendWidth, 10, legendWidth - 10, height() - debugHeight - 20);
    
    if (m_showDebugInfo) {
        m_debugArea = QRect(10, height() - debugHeight, width() - 20, debugHeight - 10);
    }
    
    // Make plot area square
    int plotSize = std::min(m_plotArea.width(), m_plotArea.height());
    m_plotArea.setSize(QSize(plotSize, plotSize));
    
    // Draw components
    drawBackground(painter);
    drawGrid(painter, m_plotArea);
    drawSatellites(painter, m_plotArea);
    drawLegend(painter, m_legendArea);
    
    if (m_showDebugInfo) {
        drawDebugInfo(painter);
    }
}

void SkyPlotWidget::drawBackground(QPainter &painter)
{
    painter.fillRect(rect(), QColor(248, 248, 248));
}

void SkyPlotWidget::drawGrid(QPainter &painter, const QRect &plotArea)
{
    painter.save();
    
    QPointF center = plotArea.center();
    int radius = plotArea.width() / 2;
    
    // Grid pen
    QPen gridPen(QColor(200, 200, 200), 1, Qt::SolidLine);
    painter.setPen(gridPen);
    
    // Draw elevation circles
    for (int elev = 30; elev <= 90; elev += 30) {
        int circleRadius = radius * (90 - elev) / 90;
        painter.drawEllipse(center, circleRadius, circleRadius);
    }
    
    // Draw horizon circle
    QPen horizonPen(QColor(80, 80, 80), 2, Qt::SolidLine);
    painter.setPen(horizonPen);
    painter.drawEllipse(center, radius, radius);
    
    // Draw azimuth lines
    painter.setPen(gridPen);
    for (int azim = 0; azim < 360; azim += 30) {
        double radians = azim * M_PI / 180.0;
        int x1 = center.x() + radius * sin(radians);
        int y1 = center.y() - radius * cos(radians);
        painter.drawLine(center, QPointF(x1, y1));
    }
    
    // Draw labels
    QFont labelFont = painter.font();
    labelFont.setPointSize(9);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(QColor(60, 60, 60));
    
    QFontMetrics fm(labelFont);
    int textRadius = radius + 15;
    
    // Cardinal directions
    painter.drawText(center.x() - fm.width("N")/2, center.y() - textRadius, "N");
    painter.drawText(center.x() + textRadius, center.y() + fm.height()/3, "E");
    painter.drawText(center.x() - fm.width("S")/2, center.y() + textRadius + fm.height(), "S");
    painter.drawText(center.x() - textRadius - fm.width("W"), center.y() + fm.height()/3, "W");
    
    // Elevation labels
    labelFont.setPointSize(7);
    labelFont.setBold(false);
    painter.setFont(labelFont);
    painter.setPen(QColor(120, 120, 120));
    
    for (int elev = 30; elev <= 60; elev += 30) {
        int circleRadius = radius * (90 - elev) / 90;
        QString label = QString("%1°").arg(elev);
        painter.drawText(center.x() + circleRadius + 3, center.y() + 3, label);
    }
    
    painter.restore();
}

void SkyPlotWidget::drawSatellites(QPainter &painter, const QRect &plotArea)
{
    painter.save();
    
    for (const auto &pair : m_satellites) {
        const SatelliteInfo &sat = *pair.second;
        
        if (!sat.isPositionValid()) continue;
        
        QPointF pos = polarToCartesian(sat.elevation, sat.azimuth, plotArea);
        QColor color = getSystemColor(sat.system);
        
        // Adjust appearance based on signal quality and status
        int satSize = getSatelliteSize(sat.cn0);
        
        // Dim satellites with poor signal or invalid tracking
        if (!sat.valid || sat.cn0 < 25.0) {
            color = color.lighter(150);
        }
        
        // Highlight hovered/selected satellites
        if (&sat == m_hoveredSatellite || &sat == m_selectedSatellite) {
            QPen highlight(Qt::white, 3);
            painter.setPen(highlight);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(pos, satSize/2 + 2, satSize/2 + 2);
        }
        
        // Draw satellite circle with position source indicator
        QPen satPen;
        switch (sat.positionSource) {
            case PositionSource::REAL:
                satPen = QPen(color.darker(120), 2, Qt::SolidLine);
                break;
            case PositionSource::COMPUTED:
                satPen = QPen(color.darker(120), 1, Qt::DashLine);
                break;
            case PositionSource::FALLBACK:
                satPen = QPen(color.darker(120), 1, Qt::DotLine);
                break;
            default:
                satPen = QPen(Qt::gray, 1, Qt::DashDotLine);
                break;
        }
        
        painter.setPen(satPen);
        painter.setBrush(QBrush(color));
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

void SkyPlotWidget::drawLegend(QPainter &painter, const QRect &legendArea)
{
    painter.save();
    
    painter.fillRect(legendArea, QColor(255, 255, 255, 230));
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(legendArea);
    
    QFont titleFont = painter.font();
    titleFont.setPointSize(9);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(Qt::black);
    
    int y = legendArea.y() + 15;
    int x = legendArea.x() + 8;
    
    // GNSS Systems
    painter.drawText(x, y, "GNSS Systems:");
    y += 20;
    
    QFont normalFont = painter.font();
    normalFont.setPointSize(8);
    normalFont.setBold(false);
    painter.setFont(normalFont);
    
    std::map<std::string, QString> systemNames = {
        {"G", "GPS"}, {"E", "Galileo"}, {"R", "GLONASS"}, {"C", "BeiDou"}
    };
    
    for (const auto &sys : systemNames) {
        QColor color = getSystemColor(sys.first);
        painter.setBrush(QBrush(color));
        painter.setPen(QPen(color.darker(150), 1));
        painter.drawEllipse(x + 2, y - 6, 8, 8);
        
        painter.setPen(Qt::black);
        painter.drawText(x + 18, y, sys.second);
        y += 14;
    }
    
    // Position Sources
    y += 10;
    painter.setFont(titleFont);
    painter.drawText(x, y, "Position Source:");
    y += 18;
    
    painter.setFont(normalFont);
    
    // Real position (solid line)
    painter.setPen(QPen(Qt::black, 2, Qt::SolidLine));
    painter.drawLine(x + 2, y - 2, x + 12, y - 2);
    painter.setPen(Qt::black);
    painter.drawText(x + 18, y, "Real");
    y += 14;
    
    // Computed position (dashed line)
    painter.setPen(QPen(Qt::black, 1, Qt::DashLine));
    painter.drawLine(x + 2, y - 2, x + 12, y - 2);
    painter.setPen(Qt::black);
    painter.drawText(x + 18, y, "Computed");
    y += 14;
    
    // Fallback position (dotted line)
    painter.setPen(QPen(Qt::black, 1, Qt::DotLine));
    painter.drawLine(x + 2, y - 2, x + 12, y - 2);
    painter.setPen(Qt::black);
    painter.drawText(x + 18, y, "Fallback");
    y += 14;
    
    // Statistics
    y += 10;
    painter.setFont(titleFont);
    painter.drawText(x, y, "Satellites:");
    y += 18;
    
    painter.setFont(normalFont);
    painter.drawText(x, y, QString("Total: %1").arg(m_totalSatellites));
    y += 12;
    painter.drawText(x, y, QString("Real: %1").arg(m_satellitesWithRealPos));
    y += 12;
    painter.drawText(x, y, QString("Computed: %1").arg(m_satellitesWithComputedPos));
    y += 12;
    painter.drawText(x, y, QString("Fallback: %1").arg(m_satellitesWithFallbackPos));
    
    painter.restore();
}

void SkyPlotWidget::drawDebugInfo(QPainter &painter)
{
    if (!m_showDebugInfo) return;
    
    painter.save();
    
    painter.fillRect(m_debugArea, QColor(240, 240, 240));
    painter.setPen(Qt::black);
    painter.drawRect(m_debugArea);
    
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    
    int x = m_debugArea.x() + 5;
    int y = m_debugArea.y() + 12;
    
    QString debugText = QString("Receiver: %1 | Update Rate: %2ms | Satellites: %3")
                       .arg(m_hasReceiverPosition ? 
                           QString("%.6f, %.6f").arg(m_receiverLat).arg(m_receiverLon) : 
                           "No Position")
                       .arg(m_updateTimer.interval())
                       .arg(m_totalSatellites);
    
    painter.drawText(x, y, debugText);
    
    if (m_selectedSatellite) {
        y += 15;
        painter.drawText(x, y, QString("Selected: %1").arg(m_selectedSatellite->getStatusString()));
    }
    
    painter.restore();
}

QColor SkyPlotWidget::getSystemColor(const std::string &system) const
{
    if (system == "G") return QColor(0, 120, 215);    // GPS - Blue
    if (system == "E") return QColor(0, 150, 0);      // Galileo - Green
    if (system == "R") return QColor(215, 0, 0);      // GLONASS - Red
    if (system == "C") return QColor(255, 140, 0);    // BeiDou - Orange
    if (system == "J") return QColor(128, 0, 128);    // QZSS - Purple
    if (system == "I") return QColor(255, 20, 147);   // IRNSS - Deep Pink
    return QColor(128, 128, 128);                      // Unknown - Gray
}

QPointF SkyPlotWidget::polarToCartesian(double elevation, double azimuth, const QRect &plotArea) const
{
    QPointF center = plotArea.center();
    int radius = plotArea.width() / 2;
    
    // Convert elevation to distance from center
    double distance = radius * (90.0 - elevation) / 90.0;
    
    // Convert azimuth to radians (0° = North, clockwise)
    double radians = azimuth * M_PI / 180.0;
    
    // Calculate Cartesian coordinates
    double x = center.x() + distance * sin(radians);
    double y = center.y() - distance * cos(radians);
    
    return QPointF(x, y);
}

int SkyPlotWidget::getSatelliteSize(double cn0) const
{
    if (cn0 > 45) return 14;
    if (cn0 > 40) return 12;
    if (cn0 > 35) return 10;
    if (cn0 > 30) return 8;
    return 6;
}

SatelliteInfo* SkyPlotWidget::findSatelliteAt(const QPointF &point)
{
    for (const auto &pair : m_satellites) {
        const SatelliteInfo &sat = *pair.second;
        if (!sat.isPositionValid()) continue;
        
        QPointF satPos = polarToCartesian(sat.elevation, sat.azimuth, m_plotArea);
        int satSize = getSatelliteSize(sat.cn0);
        
        QRectF satRect(satPos.x() - satSize/2, satPos.y() - satSize/2, satSize, satSize);
        if (satRect.contains(point)) {
            return pair.second.get();
        }
    }
    return nullptr;
}

void SkyPlotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_selectedSatellite = findSatelliteAt(event->pos());
        update();
    }
}

void SkyPlotWidget::mouseMoveEvent(QMouseEvent *event)
{
    SatelliteInfo* hoveredSat = findSatelliteAt(event->pos());
    
    if (hoveredSat != m_hoveredSatellite) {
        m_hoveredSatellite = hoveredSat;
        update();
        
        if (m_hoveredSatellite) {
            QToolTip::showText(event->globalPos(), m_hoveredSatellite->getStatusString());
        } else {
            QToolTip::hideText();
        }
    }
}

void SkyPlotWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

// Improved satellite position computation
double SkyPlotWidget::computeSatelliteElevation(int prn, const std::string& system, 
                                              double receiver_lat, double receiver_lon, double gps_time) const
{
    // Enhanced orbital mechanics simulation
    double time_factor = gps_time / 3600.0; // Convert to hours for smoother motion
    
    if (system == "G") { // GPS
        int orbital_plane = (prn - 1) % 6;
        double plane_offset = orbital_plane * M_PI / 3.0;
        double orbital_period = 12.0; // ~12 hour orbit
        double elevation = 25.0 + 50.0 * (0.5 + 0.4 * sin(2 * M_PI * time_factor / orbital_period + plane_offset));
        
        // Add some randomness based on PRN
        elevation += 10.0 * sin(prn * 0.7 + time_factor * 0.1);
        
        return std::max(5.0, std::min(85.0, elevation));
    }
    else if (system == "E") { // Galileo
        int orbital_plane = (prn - 1) % 3;
        double plane_offset = orbital_plane * 2 * M_PI / 3.0;
        double orbital_period = 14.1; // ~14.1 hour orbit
        double elevation = 30.0 + 45.0 * (0.5 + 0.35 * sin(2 * M_PI * time_factor / orbital_period + plane_offset));
        
        elevation += 8.0 * sin(prn * 0.9 + time_factor * 0.12);
        
        return std::max(10.0, std::min(80.0, elevation));
    }
    else if (system == "R") { // GLONASS
        int orbital_plane = (prn - 1) % 3;
        double plane_offset = orbital_plane * 2 * M_PI / 3.0;
        double orbital_period = 11.3; // ~11.3 hour orbit
        double elevation = 35.0 + 40.0 * (0.5 + 0.3 * sin(2 * M_PI * time_factor / orbital_period + plane_offset));
        
        elevation += 12.0 * sin(prn * 1.1 + time_factor * 0.08);
        
        return std::max(15.0, std::min(75.0, elevation));
    }
    else if (system == "C") { // BeiDou
        int orbital_plane = (prn - 1) % 3;
        double plane_offset = orbital_plane * 2 * M_PI / 3.0;
        double orbital_period = 12.9; // ~12.9 hour orbit
        double elevation = 28.0 + 47.0 * (0.5 + 0.38 * sin(2 * M_PI * time_factor / orbital_period + plane_offset));
        
        elevation += 9.0 * sin(prn * 0.8 + time_factor * 0.11);
        
        return std::max(8.0, std::min(82.0, elevation));
    }
    
    // Fallback for unknown systems
    return 30.0 + fmod(prn * 7.0, 50.0);
}

double SkyPlotWidget::computeSatelliteAzimuth(int prn, const std::string& system, 
                                            double receiver_lat, double receiver_lon, double gps_time) const
{
    double time_factor = gps_time / 3600.0;
    double azimuth = 0.0;
    
    if (system == "G") { // GPS
        int orbital_plane = (prn - 1) % 6;
        double base_azimuth = orbital_plane * 60.0;
        azimuth = base_azimuth + 45.0 * sin(2 * M_PI * time_factor / 12.0 + prn * 0.3);
        azimuth += receiver_lon * 0.15; // Longitude effect
    }
    else if (system == "E") { // Galileo
        int orbital_plane = (prn - 1) % 3;
        double base_azimuth = orbital_plane * 120.0;
        azimuth = base_azimuth + 50.0 * sin(2 * M_PI * time_factor / 14.1 + prn * 0.4);
        azimuth += receiver_lon * 0.18;
    }
    else if (system == "R") { // GLONASS
        int orbital_plane = (prn - 1) % 3;
        double base_azimuth = orbital_plane * 120.0 + 60.0; // Offset from Galileo
        azimuth = base_azimuth + 40.0 * sin(2 * M_PI * time_factor / 11.3 + prn * 0.35);
        azimuth += receiver_lon * 0.12;
    }
    else if (system == "C") { // BeiDou
        int orbital_plane = (prn - 1) % 3;
        double base_azimuth = orbital_plane * 120.0 + 30.0; // Different offset
        azimuth = base_azimuth + 48.0 * sin(2 * M_PI * time_factor / 12.9 + prn * 0.42);
        azimuth += receiver_lon * 0.16;
    }
    else {
        // Fallback
        azimuth = fmod(prn * 23.0 + time_factor * 15.0, 360.0);
    }
    
    // Normalize to 0-360 range
    while (azimuth < 0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;
    
    return azimuth;
}
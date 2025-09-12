/*!
 * \file skyplot_widget.h
 * \brief Interface of a widget that shows satellites being tracked
 * in a polar sky plot with support for real and computed satellite positions.
 *
 * \author Andrew Smilie (smilima) with assistance from  Claude Assistant, 2025.
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
#include <QDateTime>
#include <map>
#include <memory>

enum class PositionSource
{
    NONE,           // No position data available
    REAL,           // Real satellite position from GNSS-SDR
    COMPUTED,       // Computed using receiver position and time
    FALLBACK        // Fallback pattern-based position
};

struct SatelliteInfo
{
    // Basic satellite information
    int prn;
    std::string system;
    std::string signal;
    int channel_id;
    
    // Position information
    double elevation;        // degrees (0-90)
    double azimuth;          // degrees (0-360)
    PositionSource positionSource;
    
    // Signal quality
    double cn0;              // dB-Hz
    bool valid;              // tracking validity
    
    // Tracking state
    bool seenInThisUpdate;   // updated in current cycle
    int missedUpdates;       // consecutive missed updates
    QDateTime lastSeen;      // when last updated
    
    // Visual state
    bool highlighted;        // for user interaction
    
    SatelliteInfo();
    bool isPositionValid() const;
    QString getSystemName() const;
    QString getStatusString() const;
};

class SkyPlotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SkyPlotWidget(QWidget *parent = nullptr);
    ~SkyPlotWidget() override = default;

    // Configuration
    void setMaxMissedUpdates(int maxUpdates) { m_maxMissedUpdates = maxUpdates; }
    void setUpdateRate(int milliseconds) { m_updateTimer.setInterval(milliseconds); }
    void setShowDebugInfo(bool show) { m_showDebugInfo = show; update(); }

public slots:
    void updateSatellites(const gnss_sdr::Observables &observables);
    void updateReceiverPosition(const gnss_sdr::MonitorPvt &monitor_pvt);
    void clear();
    void clearStale(); // Remove satellites not seen recently

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    // Core functionality
    void processSatellite(const gnss_sdr::GnssSynchro &obs);
    void cleanupStaleSatellites();
    void scheduleUpdate();
    
    // Position computation
    bool extractRealPosition(const gnss_sdr::GnssSynchro &obs, double &elevation, double &azimuth);
    void computeApproximatePosition(const gnss_sdr::GnssSynchro &obs, double &elevation, double &azimuth);
    void computeFallbackPosition(const gnss_sdr::GnssSynchro &obs, double &elevation, double &azimuth);
    
    // Drawing functions
    void drawBackground(QPainter &painter);
    void drawGrid(QPainter &painter, const QRect &plotArea);
    void drawSatellites(QPainter &painter, const QRect &plotArea);
    void drawLegend(QPainter &painter, const QRect &legendArea);
    void drawDebugInfo(QPainter &painter);
    
    // Utility functions
    QColor getSystemColor(const std::string &system) const;
    QPointF polarToCartesian(double elevation, double azimuth, const QRect &plotArea) const;
    SatelliteInfo* findSatelliteAt(const QPointF &point);
    int getSatelliteSize(double cn0, int plotRadius) const;
    
    // Orbital mechanics (improved versions)
    double computeSatelliteElevation(int prn, const std::string& system, 
                                   double receiverLat, double receiverLon, double gpsTime) const;
    double computeSatelliteAzimuth(int prn, const std::string& system, 
                                 double receiverLat, double receiverLon, double gpsTime) const;
    
    // Data management
    std::map<int, std::unique_ptr<SatelliteInfo>> m_satellites;  // key: channel_id    

    // Layout areas
    QRect m_plotArea;
    QRect m_legendArea;
    QRect m_debugArea;
    
    // Update management
    QTimer m_updateTimer;
    bool m_needsUpdate;
    int m_maxMissedUpdates;
    
    // Receiver position for computing satellite positions
    double m_receiverLat;
    double m_receiverLon;
    double m_receiverHeight;
    double m_currentGpsTime;
    bool m_hasReceiverPosition;
    QDateTime m_lastReceiverUpdate;
    
    // Statistics
    int m_totalSatellites;
    int m_satellitesWithRealPos;
    int m_satellitesWithComputedPos;
    int m_satellitesWithFallbackPos;
    
    // User interface state
    SatelliteInfo* m_hoveredSatellite;
    SatelliteInfo* m_selectedSatellite;
    bool m_showDebugInfo;
    
    // Visual configuration
    static constexpr int MIN_WIDGET_SIZE = 300;
    static constexpr int LEGEND_WIDTH = 140;
    static constexpr int DEBUG_HEIGHT = 60;
    static constexpr int PLOT_PADDING = 0; // Padding in pixels from the edge for the horizon
    static constexpr int DEFAULT_UPDATE_INTERVAL = 100; // ms
    static constexpr int DEFAULT_MAX_MISSED_UPDATES = 5;
};

#endif  // GNSS_SDR_MONITOR_SKYPLOT_WIDGET_H_
